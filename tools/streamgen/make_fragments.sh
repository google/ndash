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

rm -f audio-192k-test-frag.mp4
rm -f 1080p-10mbps-test-frag.mp4
rm -f 720p-5mbps-test-frag.mp4
rm -f 480p-2mbps-test-frag.mp4

mp4fragment --fragment-duration 2500 --timescale 90000 audio-test-192k.mp4 audio-192k-test-frag.mp4
mp4fragment --fragment-duration 2500 --timescale 90000 1080p-10mbps-test.mp4 1080p-10mbps-test-frag.mp4
mp4fragment --fragment-duration 2500 --timescale 90000 720p-5mbps-test.mp4 720p-5mbps-test-frag.mp4
mp4fragment --fragment-duration 2500 --timescale 90000 480p-2mbps-test.mp4 480p-2mbps-test-frag.mp4

