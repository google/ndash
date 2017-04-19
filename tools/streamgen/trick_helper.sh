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

# Usage ./trick_helper index
#
# You will have to MANUALLY merge the generated adaptation set representation
# from dash_out_trick/stream.mpd into dash_out/stream.mpd and add
# <SupplementalProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
# under the adapataion set that will hold the trick streams.  Also, make sure
# that each id is unique among all the generated trick rates.
# TODO: Try to automate this step!

IDX=$1
RATE=(4 15 60 120 240)
FPS=8

# Extract frames
rm -rf tmp
mkdir tmp
ffmpeg -i 480p-2mbps-test.mp4 -vf fps=${FPS}/${RATE[$IDX]} tmp/frame-%d.png

BPS=(1600k 400k 100k 50k 25k)

# Make video with only i-frames
ffmpeg -r ${FPS}/${RATE[$IDX]} -start_number 1 -i tmp/frame-%d.png -b:v ${BPS[$IDX]} -c:v libx264 -pix_fmt yuv420p -profile:v high -g 0 480p-trick${RATE[$IDX]}.mp4

# Fragment

DUR=(10000 40000 160000 320000 640000)

rm -f 480p-trick${RATE[$IDX]}-frag.mp4
mp4fragment --fragment-duration ${DUR[$IDX]} --timescale 90000 480p-trick${RATE[$IDX]}.mp4 480p-trick${RATE[$IDX]}-frag.mp4

# Make manifest
rm -rf dash_out_trick${RATE[$IDX]}
mp4dash --no-split -o dash_out_trick${RATE[$IDX]} --max-playout-rate=lowest:${RATE[$IDX]} 480p-trick${RATE[$IDX]}-frag.mp4

mv dash_out_trick${RATE[$IDX]}/480p-trick${RATE[$IDX]}-frag.mp4 dash_out/
