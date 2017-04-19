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

project(chromium_mp4)
cmake_minimum_required(VERSION 2.8)

# Include paths relative to src directory.
include_directories(src)

# Include chromium base headers
include_directories(../base/src)

# Turn on C++11 in a way portable across CMake versions.
include(../../../cmake/Cxx11.cmake)
use_cxx11()

# Common compile flags.
include(../../../cmake/CompileFlags.cmake)

set(TARGET chromium-mp4)
list(APPEND MEDIA_SOURCES
        src/mp4/aac.cc
        src/mp4/aac.h
        src/mp4/adts_constants.cc
        src/mp4/adts_constants.h
        src/mp4/audio_codecs.cc
        src/mp4/audio_codecs.h
        src/mp4/audio_decoder_config.cc
        src/mp4/audio_decoder_config.h
        src/mp4/avc.cc
        src/mp4/avc.h
        src/mp4/bit_reader.cc
        src/mp4/bit_reader.h
        src/mp4/bit_reader_core.cc
        src/mp4/bit_reader_core.h
        src/mp4/bitstream_converter.cc
        src/mp4/bitstream_converter.h
        src/mp4/box_definitions.cc
        src/mp4/box_definitions.h
        src/mp4/box_reader.cc
        src/mp4/box_reader.h
        src/mp4/byte_queue.cc
        src/mp4/byte_queue.h
        src/mp4/channel_layout.cc
        src/mp4/channel_layout.h
        src/mp4/decoder_buffer.cc
        src/mp4/decoder_buffer.h
        src/mp4/decrypt_config.cc
        src/mp4/decrypt_config.h
        src/mp4/demuxer_stream.cc
        src/mp4/demuxer_stream.h
        src/mp4/eme_constants.h
        src/mp4/encryption_scheme.cc
        src/mp4/encryption_scheme.h
        src/mp4/es_descriptor.cc
        src/mp4/es_descriptor.h
        src/mp4/fourccs.h
        src/mp4/h264_bit_reader.cc
        src/mp4/h264_bit_reader.h
        src/mp4/h264_parser.cc
        src/mp4/h264_parser.h
        src/mp4/limits.h
        src/mp4/media_export.h
        src/mp4/media_log.cc
        src/mp4/media_log.h
        src/mp4/media_log_event.h
        src/mp4/media_util.cc
        src/mp4/media_util.h
        src/mp4/media_track.cc
        src/mp4/media_track.h
        src/mp4/media_tracks.cc
        src/mp4/media_tracks.h
        src/mp4/mp4_stream_parser.cc
        src/mp4/mp4_stream_parser.h
        src/mp4/offset_byte_queue.cc
        src/mp4/offset_byte_queue.h
        src/mp4/point.cc
        src/mp4/point.h
        src/mp4/ranges.cc
        src/mp4/ranges.h
        src/mp4/rcheck.h
        src/mp4/rect.cc
        src/mp4/rect.h
        src/mp4/sample_format.cc
        src/mp4/sample_format.h
        src/mp4/sample_to_group_iterator.cc
        src/mp4/sample_to_group_iterator.h
        src/mp4/size.cc
        src/mp4/size.h
        src/mp4/stream_parser.cc
        src/mp4/stream_parser.h
        src/mp4/stream_parser_buffer.cc
        src/mp4/stream_parser_buffer.h
        src/mp4/text_track_config.cc
        src/mp4/text_track_config.h
        src/mp4/timestamp_constants.h
        src/mp4/track_run_iterator.cc
        src/mp4/track_run_iterator.h
        src/mp4/video_codecs.cc
        src/mp4/video_codecs.h
        src/mp4/video_decoder_config.cc
        src/mp4/video_decoder_config.h
        src/mp4/video_util.cc
        src/mp4/video_util.h
        src/mp4/video_types.cc
        src/mp4/video_types.h
        )
add_library(chromium-mp4 ${MEDIA_SOURCES})

install(TARGETS chromium-mp4 ARCHIVE DESTINATION lib)
