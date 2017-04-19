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

./compile_visual.sh

rm -f 480p-2mbps-test.mp4
rm -f 720p-5mbps-test.mp4
rm -f 1080p-10mbps-test.mp4

./run_visual.sh 480 | ffmpeg -f image2pipe -codec:v:0 bmp -i pipe:0 -map 0 -r 30 -c:v libx264 -pix_fmt yuv420p -profile:v high -b:v 2M 480p-2mbps-test.mp4

./run_visual.sh 720 | ffmpeg -f image2pipe -codec:v:0 bmp -i pipe:0 -map 0 -r 30 -c:v libx264 -pix_fmt yuv420p -profile:v high -b:v 5M 720p-5mbps-test.mp4

./run_visual.sh 1080 | ffmpeg -f image2pipe -codec:v:0 bmp -i pipe:0 -map 0 -r 30 -c:v libx264 -pix_fmt yuv420p -profile:v high -b:v 10M 1080p-10mbps-test.mp4
