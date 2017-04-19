/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/util.h"

#include <map>
#include <string>

#include "base/time/time.h"

namespace ndash {

namespace util {

int64_t Util::ScaleLargeTimestamp(int64_t timestamp,
                                  int64_t multiplier,
                                  int64_t divisor) {
  if (divisor >= multiplier && (divisor % multiplier) == 0) {
    int64_t division_factor = divisor / multiplier;
    return timestamp / division_factor;
  } else if (divisor < multiplier && (multiplier % divisor) == 0) {
    int64_t multiplication_factor = multiplier / divisor;
    return timestamp * multiplication_factor;
  } else {
    double multiplication_factor = (double)multiplier / divisor;
    return (int64_t)(timestamp * multiplication_factor);
  }
}

int64_t Util::CeilDivide(int64_t numerator, int64_t denominator) {
  return (numerator + denominator - 1) / denominator;
}

// TODO(rmrossi): Convert all int64_t times in this class to use
// base::Time* instead.  Figure out how to handle special constants the
// current code expects (i.e. kNoResetPending or -1). For now, simulate the
// Java way by using the internal value from TimeTicks and convert to ms.

int64_t Util::ParseXsDateTime(const std::string& value) {
  base::Time time;
  base::Time::FromUTCString(value.c_str(), &time);
  return time.ToJavaTime();
}

int64_t Util::ParseXsDuration(const std::string& value) {
  if (value[0] != 'P') {
    return -1;
  }

  // Build scan pattern
  size_t yPos = value.find('Y', 0);
  size_t dPos = value.find('D', 0);
  size_t hPos = value.find('H', 0);
  size_t sPos = value.find('S', 0);

  size_t minPos;
  size_t monthPos;
  size_t tPos = value.find('T', 0);
  if (tPos != std::string::npos) {
    std::string date_component = value.substr(0, tPos);
    std::string time_component = value.substr(tPos);
    monthPos = date_component.find('M', 0);
    minPos = time_component.find('M', 0);
  } else {
    minPos = std::string::npos;
    monthPos = value.find('M', 0);
  }

  int num_args = 0;
  std::string pattern = "P";
  std::map<int, float> multiplier;

  if (yPos != std::string::npos) {
    pattern.append("%lfY");
    multiplier[num_args] = 31556926;
    num_args++;
  }
  if (monthPos != std::string::npos) {
    pattern.append("%lfM");
    multiplier[num_args] = 2629743.83;
    num_args++;
  }
  if (dPos != std::string::npos) {
    pattern.append("%lfD");
    multiplier[num_args] = 86400;
    num_args++;
  }
  if (hPos != std::string::npos || minPos != std::string::npos ||
      sPos != std::string::npos) {
    pattern.append("T");
  }
  if (hPos != std::string::npos) {
    pattern.append("%lfH");
    multiplier[num_args] = 3600;
    num_args++;
  }
  if (minPos != std::string::npos) {
    pattern.append("%lfM");
    multiplier[num_args] = 60;
    num_args++;
  }
  if (sPos != std::string::npos) {
    pattern.append("%lfS");
    multiplier[num_args] = 1;
    num_args++;
  }

  int num_matched = 0;
  double v[6];
  switch (num_args) {
    case 1:
      num_matched = sscanf(value.c_str(), pattern.c_str(), &v[0]);
      break;
    case 2:
      num_matched = sscanf(value.c_str(), pattern.c_str(), &v[0], &v[1]);
      break;
    case 3:
      num_matched = sscanf(value.c_str(), pattern.c_str(), &v[0], &v[1], &v[2]);
      break;
    case 4:
      num_matched =
          sscanf(value.c_str(), pattern.c_str(), &v[0], &v[1], &v[2], &v[3]);
      break;
    case 5:
      num_matched = sscanf(value.c_str(), pattern.c_str(), &v[0], &v[1], &v[2],
                           &v[3], &v[4]);
      break;
    case 6:
      num_matched = sscanf(value.c_str(), pattern.c_str(), &v[0], &v[1], &v[2],
                           &v[3], &v[4], &v[5]);
      break;
    default:
      return -1;
  }

  if (num_args != num_matched) {
    return -1;
  }

  // TODO(rmrossi): According to ISO8601 spec, only the last component may have
  // a fraction. Add a check here and fail due to parsing error if that is
  // not the case.

  int64_t milliseconds = 0;
  for (int i = 0; i < num_args; i++) {
    milliseconds += (v[i] * multiplier[i] * 1000);
  }

  return milliseconds;
}

}  // namespace util

}  // namespace ndash
