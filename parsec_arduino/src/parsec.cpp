// Copyright 2011 Google Inc.
// Author: whess@google.com (Wolfgang Hess)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <SPI.h>
#include <WProgram.h>

#include "arduino_hardware.h"
#include "dogs102lcd.h"
#include "geometry_msgs/Twist.h"
#include "odometry.h"
#include "parallax_ping.h"
#include "parsec_msgs/Colors.h"
#include "parsec_msgs/LaserTiltProfile.h"
#include "parsec_msgs/LaserTiltSignal.h"
#include "parsec_msgs/Odometry.h"
#include "position_controller.h"
#include "profiler.h"
#include "ros/node_handle.h"
#include "ros/publisher.h"
#include "rosgraph_msgs/Log.h"
#include "sensor_msgs/JointState.h"
#include "sensor_msgs/Range.h"
#include "servo_sweep.h"
#include "shift_brite.h"
#include "simple_led.h"
#include "std_msgs/Time.h"

inline float fminf(float x, float y) {
  return x < y ? x : y;
}

inline float fmaxf(float x, float y) {
  return x > y ? x : y;
}

ArduinoHardware hardware;
ros::NodeHandle node_handle(&hardware);

void SendLogMessage(const char* message) {
  node_handle.logerror(message);
}

// ----------------------------------------------------------------------
// LCD stuff
// ----------------------------------------------------------------------

DOGS102LCD display(41, 40);  // Select pin 41, data pin 40.

static void SetupDisplay() {
  display.Initialize();
  for (int i = 0; i != 8; ++i) {
    display.WriteString(0, i, "                 ");
  }

  // Activate the LCD backlight.
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
}

static void printf_row(char row, const char *format, ...) {
  static char message[18];
  va_list ap;
  va_start(ap, format);
  vsnprintf(message, 18, format, ap);
  va_end(ap);
  display.WriteString(0, row, message);
}

static void DumpProfiler(char row, const Profiler &profiler) {
  printf_row(row, "%6lu %s", profiler.GetElapsedMicros(), profiler.GetName());
}

static void WriteUART1(unsigned char byte);

// Endless loop flashing the LCD to show that we have crashed.
void Check(bool assertion, const char *format, ...) {
  if (!assertion) {
    PositionController::SoftwareEmergencyStop(&WriteUART1);
    display.WriteString(0, 3, "      ERROR      ");
    display.WriteString(0, 4, "                 ");
    static char error_message[18];
    va_list ap;
    va_start(ap, format);
    vsnprintf(error_message, 18, format, ap);
    va_end(ap);
    display.WriteString(8 - strlen(error_message) / 2, 4, error_message);
    node_handle.logerror(error_message);
    for (;;) {
      PositionController::SoftwareEmergencyStop(&WriteUART1);
      digitalWrite(8, HIGH);
      for (int i = 0; i != 500; ++i) delayMicroseconds(1000);
      digitalWrite(8, LOW);
      for (int i = 0; i != 500; ++i) delayMicroseconds(1000);
    }
  }
}

static const unsigned char large_heart[9] =
    {0x04, 0x0e, 0x1f, 0x3f, 0x7e, 0x3f, 0x1f, 0x0e, 0x04};

static const unsigned char small_heart[9] =
    {0x00, 0x04, 0x0e, 0x1e, 0x3c, 0x1e, 0x0e, 0x04, 0x00};

static void LoopDisplay() {
  static unsigned long last_heartbeat = 0;
  static bool last_heart_large = false;
  unsigned long now = micros();
  if (now - last_heartbeat > 700000l) {
    last_heartbeat = now;
    display.WritePixels(93, 0, last_heart_large ? small_heart : large_heart, 9);
    last_heart_large = !last_heart_large;
  }
  static int last_second = -1;
  int current_second = now / 1000000lu;
  if (current_second != last_second) {
    printf_row(0, "Uptime %d", current_second);
    last_second = current_second;
  }
}

static void ShowLogMessage(char* message) {
  size_t message_size = strlen(message);
  for (int i = 0; i != 4; ++i) {
    display.WriteString(0, 4 + i, "                 ");
    if (message_size > 17 * i) {
      display.WriteString(0, 4 + i, message + 17 * i);
    }
  }
}

// ----------------------------------------------------------------------
// Ultrasonic stuff
// ----------------------------------------------------------------------

const int kNumPingers = 10;
Ultrasonic pings[kNumPingers];
int current_ping = 0;
int next_ping = 0;
const float kStopDistance = 0.15f;  // Stop at 15cm. Parallax PING))) sensors work down to 2cm.
// Adapt speed to not have to stop before (in seconds). This should be roughly
// equal to (max speed / max acceleration).
const float kStopTime = 1.5f;
bool not_moving = true;
float ping_distance[kNumPingers] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 25.0f, 25.0f, 25.0f, 25.0f, 25.0f};
// All pingers, changing between front and back from one ping to another:
// const int kPingSuccessor[kNumPingers] = {5, 6, 7, 8, 9, 1, 2, 3, 4, 0};
const int kPingSuccessor[] = {2, 3, 4, 0, 1};  // Just the backwards pingers.
// >>> print ', '.join(map(str, (
// ... math.cos(2 * math.pi * i / 12)
// ... for i in (4, 5, 6, 7, 8, 10, 11, 0, 1, 2))))
//
// The values that are actually used in the code below are just
// hand-adjusted versions of the original values. The robot got very
// slow when it saw obstacles on the side, so we reduced the
// corresponding values.
//
// const float kPingerDirection[kNumPingers] = {
//     0.5f, 0.8660254f, 1.0f, 0.8660254f, 0.5f,
//     -0.5f, -0.8660254f, -1.0f, -0.8660254f, -0.5f};
const float kPingerDirection[kNumPingers] = {
    0.25f, 0.5f, 1.0f, 0.5f, 0.25f,
    -0.25f, -0.5f, -1.0f, -0.5f, -0.25f};

class ColorState {
 public:
  enum ColorType {
    kGreen, kRed, kBlue
  };

  ColorState() {
    for (int i = 0; i != 10; ++i) {
      red_[i] = 0;
      green_[i] = 0;
      blue_[i] = 0;
      best_before_[i] = 0;
    }
  }

  // Updates the color if it hasn't recently been set over ROS.
  void Update(int i, ColorType color) {
    unsigned long current = micros();
    if (static_cast<long>(current - best_before_[i]) >= 0) {
      switch (color) {
        case kGreen: {
          red_[i] = 0;
          green_[i] = 1023;
          blue_[i] = 0;
          break;
        }
        case kRed: {
          red_[i] = 1023;
          green_[i] = 0;
          blue_[i] = 0;
          break;
        }
        case kBlue: {
          red_[i] = 0;
          green_[i] = 0;
          blue_[i] = 1023;
          break;
        }
      }
      best_before_[i] = current;
    }
  }

  // Updates the color from elements of a parsec_msgs/Colors message.
  void UpdateFromMessage(const parsec_msgs::Colors& color_message) {
    unsigned long best_before = micros() + 100000ul;  // 100ms timeout.
    for (int i = 0; i != 10; ++i) {
      if (color_message.a[i]) {
        red_[i] = color_message.r[i] << 2;
        green_[i] = color_message.g[i] << 2;
        blue_[i] = color_message.b[i] << 2;
        best_before_[i] = best_before;
      }
    }
  }

  const int *GetRed() const { return red_; }
  const int *GetGreen() const { return green_; }
  const int *GetBlue() const { return blue_; }

 private:
  int red_[10];
  int green_[10];
  int blue_[10];
  unsigned long best_before_[10];
} color_state;

void ColorsCallback(const parsec_msgs::Colors& color_message) {
  color_state.UpdateFromMessage(color_message);
}

ros::Subscriber<parsec_msgs::Colors> colors_subscriber(
    "~colors", &ColorsCallback);

// Put an ultrasonic sensor reading in a ROS sensor_msgs::Range message.
void UltrasonicToMessage(float range, sensor_msgs::Range *range_message) {
  range_message->radiation_type = sensor_msgs::Range::ULTRASOUND;
  range_message->field_of_view = 0.35f;
  range_message->min_range = 0.0f;
  range_message->max_range = 100.0f;
  range_message->range = range;
}

static void SetupUltrasonic() {
  // We use PORTK for addressing.
  DDRK = 0x0f;
}

static void LoopUltrasonic() {
  if (pings[next_ping].IsReady()) {
    // Interpret the last reading. This is the median of the last 5 readings,
    // so that outliers are ignored.
    ping_distance[current_ping] = pings[current_ping].QueryDistance();

    // Continue with the next ultrasonic sensor.
    current_ping = next_ping;
    PORTK = current_ping;
    pings[current_ping].SendTriggerPulse(current_ping);
    next_ping = kPingSuccessor[current_ping];
  }

  static int last_error_count = -1;
  int current_error_count = Ultrasonic::GetErrorCount();
  if (current_error_count != last_error_count) {
    printf_row(5, "PingErrs %d", current_error_count);
    last_error_count = current_error_count;
  }
}

static void MakeUltrasonicSafe(float* velocity) {
  float safety_factor = 1.0;
  for (int i = 0; i != kNumPingers; ++i) {
    float ping_delta = *velocity * kPingerDirection[i] * kStopTime;
    float distance = ping_distance[i] - kStopDistance;
    if (ping_delta >= 0.0f ||
        distance + ping_delta >= 0.0f) {
      // Either its already moving away, or even after kStopTime this velocity
      // is not enough for this pinger reading to drop below kStopDistance.
      bool front = (kPingerDirection[i] <= 0.0f);
      bool free = (distance > 0.0f);
      color_state.Update(
          i, free ? (front ? ColorState::kBlue : ColorState::kGreen)
                  : (front ? ColorState::kRed : ColorState::kRed));
    } else {
      if (distance <= 0.0f) {
        color_state.Update(i, ColorState::kRed);
        safety_factor = 0;
      } else {
        color_state.Update(i, ColorState::kRed);
        safety_factor = fminf(safety_factor, -distance / ping_delta);
      }
    }
  }
  *velocity *= fmaxf(safety_factor, 0.0f);
}

static void ShowUltrasonicState() {
  for (int i = 0; i != kNumPingers; ++i) {
    bool front = (kPingerDirection[i] <= 0.0f);
    bool free = (ping_distance[i] > 0.6f);  // Obstacle closer than 60cm?
    color_state.Update(
        i, free ? (front ? ColorState::kGreen : ColorState::kBlue)
                : (front ? ColorState::kRed : ColorState::kRed));
  }
}

// ----------------------------------------------------------------------
// Position controller stuff
// ----------------------------------------------------------------------


// ~16 inch base (calibrated via parsec_calibration/scripts/calibrate_base_radius.py).
const float kBaseRadius = 0.18f;
// ~6 inch wheels (calibrated via parsec_calibration/scripts/calibrate_wheel_radius.py).
const float kWheelRadius = 0.07f;

float forward_velocity = 0.0;
float angular_velocity = 0.0;
unsigned long last_update = 0;
const unsigned long kTimeoutMicros = 250000ul;

Odometry odometry;
unsigned long last_odometry_update = 0;
unsigned long last_odometry_message = 0;
parsec_msgs::Odometry odometry_message;
ros::Publisher odometry_publisher("~odom_simple", &odometry_message);

static bool IsUART1Available() {
  return UCSR1A & (1 << RXC1);
}

static int ReadUART1() {
  long delay = 0;
  const int delay_step = 100;
  while (!IsUART1Available() && delay < 50000l) {
    delayMicroseconds(delay_step);
    delay += delay_step;
  }
  if (IsUART1Available()) {
    unsigned char data = UDR1;
    return data;
  } else {
    return -1;
  }
}

static void WriteUART1(unsigned char byte) {
  UCSR1B |= (1 << TXEN1);
  UCSR1B &= ~(1 << RXEN1);
  while(!(UCSR1A & (1 << UDRE1)));  // Wait until the data register is empty.
  UCSR1A |= (1 << TXC1);  // Clear the TX Complete flag by setting this bit.
  UDR1 = byte;
	while(!(UCSR1A & (1 << TXC1)));  // Wait until the byte was sent.
  UCSR1B &= ~(1 << TXEN1);
  UCSR1B |= (1 << RXEN1);
}

PositionController left_controller(&ReadUART1, &WriteUART1, 1, kWheelRadius);
PositionController right_controller(&ReadUART1, &WriteUART1, 2, kWheelRadius);

static void SetupPositionControllerParameters() {
  float gain = 0.075f;
  float acceleration = 1.0f;
  node_handle.getParam("~gain", &gain);
  node_handle.getParam("~acceleration", &acceleration);

  left_controller.SetGain(gain);
  left_controller.SetAcceleration(acceleration);
  right_controller.SetGain(gain);
  right_controller.SetAcceleration(acceleration);

  char message[40];
  snprintf(message, 40, "Gain: %d", (int) (gain * 100.0f));
  node_handle.loginfo(message);

  snprintf(message, 40, "Acceleration: %d", (int) (acceleration * 100.0f));
  node_handle.loginfo(message);
}

static void SetupPositionControllers() {
  // Position Controller Device serial port. Pins 19 (RX) and 18 (TX).
  // We depend on default for DDR, PORT, UCSR.
  UBRR1H = 0;
  UBRR1L = 51;  // 19200-ish baud from 16 MHz
  PORTD |= (1<<3);  // pull-up on TX1
  left_controller.Initialize(true);
  right_controller.Initialize(false);
}

static void UpdateOdometry(float left_odometry, float right_odometry) {
  unsigned long odometry_micros = micros();
  odometry.UpdateFromWheels(
      left_odometry, right_odometry,
      2 * kBaseRadius, 1e-6f * (odometry_micros - last_odometry_update));
  last_odometry_update = odometry_micros;
  if (odometry_micros - last_odometry_message > 70000ul) {
    odometry.ToMessage(node_handle, &odometry_message);
    odometry_publisher.publish(&odometry_message);
    last_odometry_message = odometry_micros;
  }
}

static void LoopPositionController() {
  if (micros() - last_update > kTimeoutMicros) {
    forward_velocity = 0.0f;
    angular_velocity = 0.0f;
    last_update = micros();
  }
  float safe_velocity = forward_velocity;
  if (abs(safe_velocity) > 2e-1f) {
    not_moving = false;
  } else if (abs(safe_velocity) < 2e-2f) {
    not_moving = true;
  }
  if (safe_velocity >= 0.0f) {
    // Moving forward, i.e., move_base-controlled. We just show where we see
    // obstacles.
    ShowUltrasonicState();
  } else {
    MakeUltrasonicSafe(&safe_velocity);
  }
  if (not_moving && abs(angular_velocity) > 1e-6f) {
    for (int i = 0; i != kNumPingers; ++i) {
      if (color_state.GetRed()[i] == 0) {
        color_state.Update(i, ColorState::kBlue);
      }
    }
  }
  float left_velocity = safe_velocity - kBaseRadius * angular_velocity;
  float right_velocity = safe_velocity + kBaseRadius * angular_velocity;
  float left_odometry = left_controller.UpdateVelocity(-left_velocity);
  float right_odometry = right_controller.UpdateVelocity(-right_velocity);
  UpdateOdometry(-left_odometry, -right_odometry);
}

// ----------------------------------------------------------------------
// Tilting servo
// ----------------------------------------------------------------------

parsec_msgs::LaserTiltSignal tilt_signal;
ros::Publisher tilt_signal_pub("~signal", &tilt_signal);
unsigned long last_servo_joint_state_message = 0;

void PublishLaserSignal(int signal) {
  tilt_signal.header.stamp = node_handle.now();
  tilt_signal.signal = signal;
  tilt_signal_pub.publish(&tilt_signal);
  // HACK(damonkohler): Force a publish of the joint state message.
  last_servo_joint_state_message = 0;
}

ServoSweep servo_sweep(&PublishLaserSignal);  // Using pin 11 and timer 1.

void TiltProfileCallback(const parsec_msgs::LaserTiltProfile &tilt_profile_msg) {
  servo_sweep.SetProfile(tilt_profile_msg.min_angle, tilt_profile_msg.max_angle,
                         tilt_profile_msg.increasing_duration, tilt_profile_msg.decreasing_duration);
  char message[40];
  sprintf(message, "Servo profile: %d %d %d %d",
      (int) (tilt_profile_msg.min_angle * 100.0f),
      (int) (tilt_profile_msg.max_angle * 100.0f),
      (int) (tilt_profile_msg.increasing_duration * 100.0f),
      (int) (tilt_profile_msg.decreasing_duration * 100.0f));
  node_handle.loginfo(message);
}

ros::Subscriber<parsec_msgs::LaserTiltProfile> tilt_profile_subscriber(
  "~profile", &TiltProfileCallback);

// These are constants are configured for the Modelcraft MC-621.
// kMinAngle and kMaxAngle should correspond to the servo's position at
// kMinPwmPeriod and kMaxPwmPeriod repsectively.
static const unsigned int kServoMinPwmPeriod = 800;
static const float kServoMinAngle = -1.0028632386446623;
static const unsigned int kServoMaxPwmPeriod = 2100;
static const float kServoMaxAngle = 1.2274247888469814;
static const int kServoIncreasingPhaseOffset = 0;
static const int kServoDecreasingPhaseOffset = 0;

void SetupServoSweep() {
  servo_sweep.Attach();
  // NOTE(damonkohler): Servo PWM periods are typically between 500 and 2500.
  // The conversion later from signed to unsigned should be safe.
  int pwm_periods[2] = { kServoMinPwmPeriod, kServoMaxPwmPeriod };
  float angles[2] = { kServoMinAngle, kServoMaxAngle };
  int phase_offsets[2] = { kServoIncreasingPhaseOffset, kServoDecreasingPhaseOffset };
  node_handle.getParam("~servo_pwm_periods", pwm_periods, 2);
  node_handle.getParam("~servo_angles", angles, 2);
  node_handle.getParam("~servo_phase_offsets", phase_offsets, 2);
  servo_sweep.SetParameters(pwm_periods[0], pwm_periods[1],
      angles[0], angles[1], phase_offsets[0], phase_offsets[1]);

  char message[40];
  snprintf(message, 40, "Servo values: %d %d %d %d %d %d",
           pwm_periods[0], pwm_periods[1],
           (int) (angles[0] * 100.0f), (int) (angles[1] * 100.0f),
           phase_offsets[0], phase_offsets[1]);
  node_handle.loginfo(message);
}

sensor_msgs::JointState servo_joint_state_message;
ros::Publisher servo_joint_state_publisher("joint_states", &servo_joint_state_message);

void PublishJointState(float servo_position) {
  static float positions[1];
  positions[0] = servo_position;
  // Note: this name need to match the URDF for the
  // robot_state_publisher work correctly.
  // See parsec_description/robots/parsec.urdf
  static char name[] = "tilt_laser_joint";
  static char *names[] = {name};
  float zeros[] = {0.0f};
  servo_joint_state_message.header.stamp =
      node_handle.now() + ros::Duration::fromMillis(servo_sweep.GetPhaseOffset());
  servo_joint_state_message.name_length = 1;
  servo_joint_state_message.name = names;
  servo_joint_state_message.position_length = 1;
  servo_joint_state_message.position = positions;
  servo_joint_state_message.velocity_length = 1;
  servo_joint_state_message.velocity = zeros;
  servo_joint_state_message.effort_length = 1;
  servo_joint_state_message.effort = zeros;
  servo_joint_state_publisher.publish(&servo_joint_state_message);
}

void LoopServoSweep() {
  float position = servo_sweep.Update();
  unsigned long servo_micros = micros();
  if (servo_micros - last_servo_joint_state_message > 70000ul) {
    PublishJointState(position);
    last_servo_joint_state_message = servo_micros;
  }
}

// ----------------------------------------------------------------------
// ROS serial communication
// ----------------------------------------------------------------------

void VelocityCallback(const geometry_msgs::Twist& velocity_message) {
  forward_velocity = velocity_message.linear.x;
  angular_velocity = velocity_message.angular.z;
  last_update = micros();
}

ros::Subscriber<geometry_msgs::Twist> velocity_subscriber(
    "cmd_vel", &VelocityCallback);

static void SetupROSSerial() {
  node_handle.subscribe(velocity_subscriber);
  node_handle.subscribe(tilt_profile_subscriber);
  node_handle.subscribe(colors_subscriber);
  node_handle.advertise(tilt_signal_pub);
  node_handle.advertise(odometry_publisher);
  node_handle.advertise(servo_joint_state_publisher);
}

static void LoopROSSerial() {
  node_handle.spinOnce();
  static int last_invalid_size_error_count = -1;
  int current_invalid_size_error_count = node_handle.getInvalidSizeErrorCount();
  if (current_invalid_size_error_count != last_invalid_size_error_count) {
    printf_row(1, "SizeErrs %d", current_invalid_size_error_count);
    last_invalid_size_error_count = current_invalid_size_error_count;
  }
  static int last_checksum_error_count = -1;
  int current_checksum_error_count = node_handle.getChecksumErrorCount();
  if (current_checksum_error_count != last_checksum_error_count) {
    printf_row(2, "CksmErrs %d", current_checksum_error_count);
    last_checksum_error_count = current_checksum_error_count;
  }
  static int last_state_error_count = -1;
  int current_state_error_count = node_handle.getChecksumErrorCount();
  if (current_state_error_count != last_state_error_count) {
    printf_row(3, "StateErrs %d", current_state_error_count);
    last_state_error_count = current_state_error_count;
  }
  static int last_malformed_message_error_count = -1;
  int current_malformed_message_error_count = node_handle.getMalformedMessageErrorCount();
  if (current_malformed_message_error_count != last_malformed_message_error_count) {
    printf_row(4, "MsgErrs %d", current_malformed_message_error_count);
    last_malformed_message_error_count = current_malformed_message_error_count;
  }
}

// ----------------------------------------------------------------------
// ShiftBrite stuff
// ----------------------------------------------------------------------

ShiftBrite shift_brite(42);  // Latch pin 42.

static void SetupShiftBrite() {
  pinMode(13, INPUT);
  pinMode(10, INPUT);
  shift_brite.Initialize(10);
  shift_brite.Enable(43);  // Enable pin 43.
}

static void LoopShiftBrite() {
  // Only initialize every so often to double performance, cutting time from
  // 1276 us to 700 us.
  static int initialize_next_at = 0;
  if (initialize_next_at == 0) {
    shift_brite.Initialize(10);
    initialize_next_at = 10;
  } else {
    --initialize_next_at;
  }
  shift_brite.UpdateColors(
      10, color_state.GetRed(), color_state.GetGreen(), color_state.GetBlue());
}

// ----------------------------------------------------------------------

void setup() {
  // TODO(moesenle): Why is SPI initialization actually necessary to get
  // ROSSerial working?
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE3);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SetupDisplay();
  SetupROSSerial();
  SetupUltrasonic();
  SetupShiftBrite();
  SetupPositionControllers();

  // Wait until we've connected to the host.
  printf_row(7, "Waiting");
  hardware.init();
  while (!node_handle.connected()) {
    LoopROSSerial();
    LoopDisplay();
  }
  printf_row(7, "Connected");

  SetupPositionControllerParameters();
  SetupServoSweep();
}

void loop() {
  LoopDisplay();
  LoopROSSerial();
  LoopServoSweep();
  LoopPositionController();
  LoopServoSweep();
  LoopUltrasonic();
  LoopShiftBrite();
  LoopServoSweep();
}
