// Copyright 2011 Google Inc.
// Author: moesenle@google.com (Lorenz Moesenlechner)
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

#include "rcconsole/include_name_filter.h"

#include <pluginlib/class_list_macros.h>

namespace rcconsole {

bool IncludeNameFilter::FilterLog(const rosgraph_msgs::Log &log) {
  return !boost::regex_match(log.name, regex());
}

}  // namespace rcconsole

PLUGINLIB_DECLARE_CLASS(rcconsole, IncludeNameFilter, rcconsole::IncludeNameFilter, nodelet::Nodelet)