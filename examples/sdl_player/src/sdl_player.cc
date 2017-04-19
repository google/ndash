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

#include <iostream>
#include <memory>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "player.h"
#include "util/statusor.h"

DEFINE_string(dash_url, "", "MPEG-DASH manifest URL to load");

int main(int argc, char** argv) {
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_dash_url.empty()) {
    LOG(ERROR) << "No --dash_url provided";
    return 1;
  }

  auto player_result = dash::player::Player::Make(FLAGS_dash_url);
  if (!player_result.ok()) {
    LOG(ERROR) << "Unable to create DASH player: " << player_result.status();
    return 1;
  }

  auto player = player_result.ConsumeValueOrDie();
  player->Start();

  return 0;
}
