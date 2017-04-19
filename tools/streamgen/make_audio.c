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

#include <math.h>
#include <stdio.h>

// Generate PCM_16LE audio stream with a beep every second.
// Output goes to stdout and it is expected this is captured into a file
// for processing.
int main(int argc, char* argv[]) {
   long i;
   double R = 48000;
   double C = 261.62;
   int j;
   for (j = 0 ; j < 7200; j++) {
     for (i = 0 ; i < 4800; i++) {
       double v = sin(i*2*M_PI/R*C);
       short v2 = v * 127.0;
       printf ("%c%c", (v2 << 8) & 0xff, v2 & 0xff);
     }
     for (i = 0 ; i < 48000-4800; i++) {
       double v = 0;
       short v2 = v * 127.0;
       printf ("%c%c", (v2 << 8) & 0xff, v2 & 0xff);
     }
   }
}
