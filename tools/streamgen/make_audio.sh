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

rm -f audio-test-192k.mp4
gcc make_audio.c -lm -o make_audio

./make_audio > audio.pcm

ffmpeg -f s16le -acodec pcm_s16le -ac 1 -ar 48000 -i audio.pcm -vn -c:a aac -q 1 -b:a 192k -strict experimental audio-test-192k.mp4
