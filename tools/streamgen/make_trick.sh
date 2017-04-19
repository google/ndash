#!/bin/sh

# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# 4x
./trick_helper.sh 0
# 15x
./trick_helper.sh 1
# 60x
./trick_helper.sh 2
# 120x
./trick_helper.sh 3
# 240x
./trick_helper.sh 4

echo Manual step required: Manually edit dash_out/stream.mpd and
echo create a new adaptation set that will hold all trick
echo representations from dash_out_trick#/stream.mpd.  Add the
echo following supplemental property to that adaptation set:
echo '<SupplementalProperty'
echo '    schemeIdUri="http://dashif.org/guidelines/trickmode"'
echo '    value="1"/>'
echo Also, make sure to change the representation ids so that they
echo are unique among all the generated trick representations.

# TODO: Try to automate this step!
