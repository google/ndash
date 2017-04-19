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

#include <curl/curl.h>

#include <gflags/gflags.h>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "gtest/gtest.h"

int main(int ac, char* av[]) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(ac, av);

  logging::LoggingSettings logging_settings;
  InitLogging(logging_settings);

  testing::InitGoogleTest(&ac, av);

  gflags::ParseCommandLineFlags(&ac, &av, false);

  curl_global_init(CURL_GLOBAL_ALL);

  int exit_code = RUN_ALL_TESTS();

  curl_global_cleanup();

  return exit_code;
}
