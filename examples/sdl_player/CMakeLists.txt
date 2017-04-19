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

project(sdl_player)
cmake_minimum_required(VERSION 2.8)

# Include paths relative to src directory.
include_directories(src)

# Include paths relative to src directory.
include_directories(../../ndash/src)

# Find some packages.
find_package(GTest REQUIRED)
find_package(gflags REQUIRED)

include(FindPkgConfig)
pkg_search_module(AVUTIL libavutil REQUIRED)
pkg_search_module(AVFORMAT libavformat REQUIRED)
pkg_search_module(AVCODEC libavcodec REQUIRED)
pkg_search_module(SWSCALE libswscale REQUIRED)
pkg_search_module(SWRESAMPLE libswresample REQUIRED)
pkg_search_module(SDL2 sdl2 REQUIRED)
pkg_search_module(SDL2_MIXER SDL2_mixer REQUIRED)
pkg_search_module(LIBEVENT libevent REQUIRED)
pkg_search_module(XML2 libxml-2.0 REQUIRED)
pkg_search_module(LIBCURL libcurl REQUIRED)
pkg_search_module(STRINGENCODERS stringencoders REQUIRED)

find_library(GLOG_LIBRARY NAMES glog)
if(NOT GLOG_LIBRARY)
    message(FATAL_ERROR "GLog library not found")
endif(NOT GLOG_LIBRARY)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GFLAGS_INCLUDE_DIRS})
include_directories(${AVUTIL_INCLUDE_DIRS})
include_directories(${SDL2_INCLUDE_DIRS})
link_directories(${SWSCALE_LIBRARY_DIRS})
link_directories(${LIBEVENT_LIBRARY_DIRS})
link_directories(${STRINGENCODERS_LIBRARY_DIRS})

message(STATUS "Project root: ${PROJECT_ROOT}" ${PROJECT_SOURCE_DIR})
message(STATUS "PKG_CONFIG_SYSROOT_DIR: $ENV{PKG_CONFIG_SYSROOT_DIR}")
message(STATUS "PKG_CONFIG_PATH: $ENV{PKG_CONFIG_PATH}")
message(STATUS "AVUTIL_INCLUDE_DIRS: ${AVUTIL_INCLUDE_DIRS}")
message(STATUS "AVUTIL_LIBRARIES: ${AVUTIL_LIBRARIES} in ${AVUTIL_LIBRARY_DIRS}")
message(STATUS "SWSCALE_LIBRARIES: ${SWSCALE_LIBRARIES} in ${SWSCALE_LIBRARY_DIRS}")

# Enable the possibility of executing unit tests.
enable_testing()

# Turn on C++11 in a way portable across CMake versions.
include(../../cmake/Cxx11.cmake)
use_cxx11()

# Common compile flags.
include(../../cmake/CompileFlags.cmake)

set(TARGET "native-player")
list(APPEND PLAYER_SOURCES
        src/util/byte_buffer.cc
        src/util/status.cc
        src/util/statusor.cc
        src/frame_source_queue.cc
        src/ndash_stream.cc
        src/player.cc
        )
list(APPEND PLAYER_HEADERS
        src/util/byte_buffer.h
        src/util/status.h
        src/util/statusor.h
        src/frame_source_queue.h
        src/ndash_stream.h
        src/player.h
        )
set_source_files_properties(${PLAYER_HEADERS} PROPERTIES HEADER_FILE_ONLY "TRUE")
add_library(native-player-lib ${PLAYER_SOURCES} ${PLAYER_HEADERS})

# Function to declare unit tests.
function(define_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name}
            native-player-lib
            ${GTEST_BOTH_LIBRARIES}
            ${GLOG_LIBRARY}
            pthread)
    add_test(${name} ${name})
endfunction(define_test)

define_test(byte_buffer_test src/util/byte_buffer_test.cc)
define_test(status_test src/util/status_test.cc)

add_executable(sdl_player src/sdl_player.cc)
add_dependencies(sdl_player chromium-base chromium-mp4)
target_link_libraries(sdl_player
        native-player-lib
        ndash
        ${AVFORMAT_LIBRARIES}
        ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWSCALE_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
        ${LIBEVENT_LIBRARIES}
        ${LIBCURL_LIBRARIES}
        ${XML2_LIBRARIES}
        ${STRINGENCODERS_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${SDL2_MIXER_LIBRARIES}
        bz2
        ${GLOG_LIBRARY}
        ${GFLAGS_LIBRARIES}
        pthread
        dl
        z
        )
