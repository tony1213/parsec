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

#include "dogs102lcd.h"

#include <SPI.h>
#include <WProgram.h>

// We use font-schumacher-misc-X11R7.0-1.0.0 of xfonts-base, its license says:

// Copyright 1989 Dale Schumacher, dal@syntel.mn.org
//                399 Beacon Ave.
//                St. Paul, MN  55104-3527
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted, provided
// that the above copyright notice appear in all copies and that both that
// copyright notice and this permission notice appear in supporting
// documentation, and that the name of Dale Schumacher not be used in
// advertising or publicity pertaining to distribution of the software without
// specific, written prior permission.  Dale Schumacher makes no
// representations about the suitability of this software for any purpose.  It
// is provided "as is" without express or implied warranty.
const unsigned char DOGS102LCD::font_6x8[128][6] = {
    {0x40, 0xff, 0xc5, 0xc5, 0xc5, 0xff},
    {0x00, 0x70, 0x28, 0x24, 0x28, 0x70},
    {0x00, 0x7c, 0x54, 0x54, 0x54, 0x28},
    {0x00, 0x38, 0x44, 0x44, 0x44, 0x28},
    {0x00, 0x7c, 0x44, 0x44, 0x44, 0x38},
    {0x00, 0x7c, 0x54, 0x54, 0x54, 0x44},
    {0x00, 0x7c, 0x14, 0x14, 0x14, 0x04},
    {0x00, 0x38, 0x44, 0x44, 0x54, 0x74},
    {0x00, 0x7c, 0x10, 0x10, 0x10, 0x7c},
    {0x00, 0x00, 0x44, 0x7c, 0x44, 0x00},
    {0x00, 0x20, 0x40, 0x44, 0x44, 0x3c},
    {0x00, 0x7c, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x7c, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x7c, 0x08, 0x30, 0x08, 0x7c},
    {0x00, 0x7c, 0x08, 0x10, 0x20, 0x7c},
    {0x00, 0x38, 0x44, 0x44, 0x44, 0x38},
    {0x00, 0x7c, 0x14, 0x14, 0x14, 0x08},
    {0x00, 0x38, 0x44, 0x44, 0xc4, 0xb8},
    {0x00, 0x7c, 0x14, 0x34, 0x54, 0x48},
    {0x00, 0x48, 0x54, 0x54, 0x54, 0x24},
    {0x00, 0x04, 0x04, 0x7c, 0x04, 0x04},
    {0x00, 0x3c, 0x40, 0x40, 0x40, 0x3c},
    {0x00, 0x0c, 0x30, 0x40, 0x30, 0x0c},
    {0x00, 0x7c, 0x20, 0x18, 0x20, 0x7c},
    {0x00, 0x44, 0x28, 0x10, 0x28, 0x44},
    {0x00, 0x04, 0x08, 0x70, 0x08, 0x04},
    {0x00, 0x44, 0x64, 0x54, 0x4c, 0x44},
    {0x1f, 0x15, 0x15, 0x70, 0x88, 0x88},
    {0x00, 0x08, 0x0c, 0x7e, 0x0c, 0x08},
    {0x00, 0x10, 0x30, 0x7e, 0x30, 0x10},
    {0x08, 0x08, 0x08, 0x3e, 0x1c, 0x08},
    {0x08, 0x1c, 0x3e, 0x08, 0x08, 0x08},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x5f, 0x00, 0x00},
    {0x00, 0x00, 0x03, 0x00, 0x03, 0x00},
    {0x00, 0x14, 0x3e, 0x14, 0x3e, 0x14},
    {0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12},
    {0x00, 0x23, 0x13, 0x08, 0x64, 0x62},
    {0x00, 0x36, 0x49, 0x51, 0x21, 0x50},
    {0x00, 0x00, 0x02, 0x01, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x1c, 0x22, 0x41},
    {0x00, 0x41, 0x22, 0x1c, 0x00, 0x00},
    {0x00, 0x14, 0x08, 0x3e, 0x08, 0x14},
    {0x00, 0x08, 0x08, 0x3e, 0x08, 0x08},
    {0x00, 0x00, 0x80, 0x60, 0x00, 0x00},
    {0x00, 0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x00, 0x00, 0x60, 0x00, 0x00},
    {0x00, 0x00, 0xc0, 0x30, 0x0c, 0x03},
    {0x00, 0x3e, 0x51, 0x49, 0x45, 0x3e},
    {0x00, 0x00, 0x02, 0x7f, 0x00, 0x00},
    {0x00, 0x42, 0x61, 0x51, 0x49, 0x46},
    {0x00, 0x22, 0x41, 0x49, 0x49, 0x36},
    {0x00, 0x10, 0x1c, 0x53, 0x7f, 0x50},
    {0x00, 0x27, 0x45, 0x45, 0x45, 0x39},
    {0x00, 0x3c, 0x4a, 0x49, 0x49, 0x30},
    {0x00, 0x03, 0x01, 0x61, 0x19, 0x07},
    {0x00, 0x36, 0x49, 0x49, 0x49, 0x36},
    {0x00, 0x06, 0x49, 0x49, 0x29, 0x1e},
    {0x00, 0x00, 0x00, 0x66, 0x00, 0x00},
    {0x00, 0x00, 0x80, 0x66, 0x00, 0x00},
    {0x08, 0x08, 0x14, 0x14, 0x22, 0x22},
    {0x00, 0x14, 0x14, 0x14, 0x14, 0x14},
    {0x22, 0x22, 0x14, 0x14, 0x08, 0x08},
    {0x00, 0x02, 0x01, 0x51, 0x09, 0x06},
    {0x00, 0x3e, 0x41, 0x5d, 0x5d, 0x0e},
    {0x00, 0x7c, 0x12, 0x11, 0x12, 0x7c},
    {0x00, 0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x00, 0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x00, 0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x00, 0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x00, 0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x00, 0x3e, 0x41, 0x41, 0x49, 0x7a},
    {0x00, 0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0x00, 0x41, 0x41, 0x7f, 0x41, 0x41},
    {0x00, 0x30, 0x40, 0x41, 0x41, 0x3f},
    {0x00, 0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x00, 0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x7f, 0x02, 0x0c, 0x02, 0x7f},
    {0x00, 0x7f, 0x06, 0x08, 0x30, 0x7f},
    {0x00, 0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x00, 0x7f, 0x09, 0x09, 0x09, 0x06},
    {0x00, 0x3e, 0x41, 0x41, 0xc1, 0xbe},
    {0x00, 0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x00, 0x26, 0x49, 0x49, 0x49, 0x32},
    {0x00, 0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x00, 0x3f, 0x40, 0x40, 0x40, 0x3f},
    {0x00, 0x07, 0x18, 0x60, 0x18, 0x07},
    {0x00, 0x7f, 0x20, 0x18, 0x20, 0x7f},
    {0x00, 0x63, 0x14, 0x08, 0x14, 0x63},
    {0x00, 0x03, 0x04, 0x78, 0x04, 0x03},
    {0x00, 0x61, 0x51, 0x49, 0x45, 0x43},
    {0x00, 0x00, 0x00, 0x7f, 0x41, 0x41},
    {0x00, 0x00, 0x03, 0x0c, 0x30, 0xc0},
    {0x00, 0x41, 0x41, 0x7f, 0x00, 0x00},
    {0x00, 0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x00, 0x00, 0x01, 0x02, 0x00},
    {0x00, 0x38, 0x44, 0x44, 0x24, 0x7c},
    {0x00, 0x7f, 0x44, 0x44, 0x44, 0x38},
    {0x00, 0x38, 0x44, 0x44, 0x44, 0x44},
    {0x00, 0x38, 0x44, 0x44, 0x44, 0x7f},
    {0x00, 0x38, 0x54, 0x54, 0x54, 0x18},
    {0x00, 0x04, 0x7e, 0x05, 0x05, 0x01},
    {0x00, 0x18, 0xa4, 0xa4, 0xa4, 0x7c},
    {0x00, 0x7f, 0x04, 0x04, 0x04, 0x78},
    {0x00, 0x00, 0x44, 0x7d, 0x40, 0x00},
    {0x00, 0x80, 0x84, 0x84, 0x7d, 0x00},
    {0x00, 0x7f, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x00, 0x41, 0x7f, 0x40, 0x00},
    {0x00, 0x7c, 0x04, 0x38, 0x04, 0x78},
    {0x00, 0x7c, 0x08, 0x04, 0x04, 0x78},
    {0x00, 0x38, 0x44, 0x44, 0x44, 0x38},
    {0x00, 0xfc, 0x44, 0x44, 0x44, 0x38},
    {0x00, 0x38, 0x44, 0x44, 0x44, 0xfc},
    {0x00, 0x7c, 0x08, 0x04, 0x04, 0x00},
    {0x00, 0x48, 0x54, 0x54, 0x54, 0x24},
    {0x00, 0x04, 0x04, 0x3f, 0x44, 0x44},
    {0x00, 0x3c, 0x40, 0x40, 0x20, 0x7c},
    {0x00, 0x04, 0x1c, 0x60, 0x1c, 0x04},
    {0x00, 0x3c, 0x40, 0x38, 0x40, 0x3c},
    {0x00, 0x44, 0x28, 0x10, 0x28, 0x44},
    {0x00, 0x1c, 0xa0, 0xa0, 0xa0, 0x7c},
    {0x00, 0x44, 0x64, 0x54, 0x4c, 0x44},
    {0x00, 0x00, 0x00, 0x08, 0x36, 0x41},
    {0x00, 0x00, 0x00, 0x7f, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00, 0x00},
    {0x00, 0x02, 0x01, 0x02, 0x04, 0x02},
    {0x00, 0x60, 0x58, 0x46, 0x58, 0x60}};

DOGS102LCD::DOGS102LCD(int select_pin, int data_pin)
    : select_pin_(select_pin), data_pin_(data_pin) {
  pinMode(select_pin_, OUTPUT);
  pinMode(data_pin_, OUTPUT);
  digitalWrite(select_pin_, HIGH);
  digitalWrite(data_pin_, LOW);
}

void DOGS102LCD::Initialize() {
  digitalWrite(select_pin_, LOW);
  SPI.transfer(0x40);  // Display start line 0
  SPI.transfer(0xA0);  // SEG normal
  SPI.transfer(0xC8);  // COM reverse
  SPI.transfer(0xA6);  // Display inverse off
  SPI.transfer(0xA2);  // Bias to 1/9
  SPI.transfer(0x2F);  // Booster, Regulator, Follower on
  SPI.transfer(0x27);  // Set contrast
  SPI.transfer(0x81);  // "
  SPI.transfer(0x0c);  // "
  SPI.transfer(0xFA);  // Temperature compensation
  SPI.transfer(0x93);  // "
  SPI.transfer(0xAF);  // Display on
  digitalWrite(select_pin_, HIGH);
}

void DOGS102LCD::WriteCharacter(char column, char row, char ascii) {
  if (static_cast<unsigned char>(ascii) > 0x7f) {
    ascii = ' ';
  }
  digitalWrite(select_pin_, LOW);
  SetPage(row);
  SetColumn(column * 6);
  digitalWrite(data_pin_, HIGH);
  SPI.transfer(font_6x8[ascii][0]);
  SPI.transfer(font_6x8[ascii][1]);
  SPI.transfer(font_6x8[ascii][2]);
  SPI.transfer(font_6x8[ascii][3]);
  SPI.transfer(font_6x8[ascii][4]);
  SPI.transfer(font_6x8[ascii][5]);
  digitalWrite(data_pin_, LOW);
  digitalWrite(select_pin_, HIGH);
  for (int i = 0; i != 40; ++i) {
    SPI.transfer(0x00);
  }
}

void DOGS102LCD::WriteString(char column, char row, const char* text) {
  for (; *text != 0 && column < 17; ++text, ++column) {
    WriteCharacter(column, row, *text);
  }
}

void DOGS102LCD::WritePixels(
    char column, char row, const unsigned char* bitmap, char n) {
  digitalWrite(select_pin_, LOW);
  SetPage(row);
  SetColumn(column);
  digitalWrite(data_pin_, HIGH);
  for (; n > 0; --n, bitmap++) {
    SPI.transfer(*bitmap);
  }
  digitalWrite(data_pin_, LOW);
  digitalWrite(select_pin_, HIGH);
}

void DOGS102LCD::SetPage(unsigned char page) {
  SPI.transfer(0xB0 | page);
}

void DOGS102LCD::SetColumn(unsigned char column) {
  column += 30;  // First column address is 30 for 12 o'clock orientation.
  SPI.transfer(column & 0xf);
  SPI.transfer((column >> 4) + 0x10);
}
