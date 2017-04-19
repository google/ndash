Native DASH Player Stream Gen Tool
==================================

These scripts can generate a 2 hour DASH stream test pattern video with
480p, 720p and 1080p resolutions.  A manual step to merge in the trick
streams is required at the end. See make_trick.sh.

## Dependencies

You will need the following tools on your path:

* javac 1.8+
* ffmpeg-3.2.1+ compiled with libx264 support
* Bento4-SDK-1-5-0-613

## Create the stream

    ./make_all.sh

## TODOs

* Audio is not properly synced to video yet. Figure out a way to generate
  PTS values from the generated audio.pcm that will stay in sync with the
  generated video.
