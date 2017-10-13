libndash README
===============

This is not an official Google product.

## libndash

libndash is a C++ library that provides all the functionality one would
need to build an adaptive streaming media player (not including
decoding/rendering frames to a display).  It provides the following
functionality:

* download and parse DASH manifests
* download segments from fragmented mp4 audio/video streams
* parse video/audio frames from segments
* deliver frames to a client decoder
* switch to different representations based on network conditions
* notify the client when protected content is encountered
* provide seek/jump and trick play functionality (forward/backward)

This project is based on a translation of ExoPlayer v1 source files
(https://github.com/google/ExoPlayer/tree/release-v1) with non-DASH related
functionality removed. It is best suited for environments that require a
native code solution for adaptive streaming.

## sdl_player

sdl_player is a sample media player written on top of libndash. It
demonstrates how to implement the final steps of decoding and rendering frames
delivered by libndash using ffmpeg/SDL/Alsa.

## Libraries

For Debian-type systems, the following packages should be installed:

* libevent-dev
* libmodpbase64-dev
* libpthread-stubs0-dev
* libxml2-dev
* libcurl4-gnutls-dev
* libsdl2-dev
* libsdl2-mixer-dev
* libbz2-dev

Copy and paste:

apt-get install libevent-dev libmodpbase64-dev libpthread-stubs0-dev libxml2-dev \
 libcurl4-gnutls-dev libsdl2-dev libsdl2-mixer-dev libbz2-dev

If you are using a recent Debian/Ubuntu with an up to date ffmpeg:

* libavcodec-dev
* libavdevice-dev
* libavfilter-dev
* libavformat-dev
* libavresample-dev
* libavresample3
* libavutil-dev
* libswresample-dev
* libswscale-dev
* libgoogle-glog-dev
* libgflags-dev

Copy and paste:

apt-get install libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavresample-dev \
  libavresample3 libavutil-dev libswresample-dev libswscale-dev libgoogle-glog-dev libgflags-dev

Otherwise you will need to install the following from source:

* ffmpeg (https://github.com/FFmpeg/FFmpeg)
* glog (https://github.com/google/glog)
* gflags (https://github.com/gflags/gflags)

  NOTE: If your ffmpeg was compiled with --enable-libx264, you will need to
        add x264 to CMakeLists.txt link rules for sdl_player.

To build:

From the top directory:

cmake .
make -jX  # where X is the number of cores on your machine or around there

To run the unit tests:

From the top directory:

./build/ndash_unittests --test_data_path=`pwd`/ndash/src

## License

This software is licensed under Apache Software License, Version 2.0
