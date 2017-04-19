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

project(ndash)
cmake_minimum_required(VERSION 2.8)

# Include paths relative to src directory.
include_directories(src)

# Include chromium base and mp4 heaader paths.
include_directories(../third_party/chromium/base/src)
include_directories(../third_party/chromium/mp4/src)

# Find some packages.
include(FindPkgConfig)
pkg_search_module(LIBEVENT libevent REQUIRED)
pkg_search_module(XML2 libxml-2.0 REQUIRED)
pkg_search_module(LIBCURL libcurl REQUIRED)
pkg_search_module(STRINGENCODERS stringencoders REQUIRED)

include_directories(${XML2_INCLUDE_DIRS})
include_directories(${STRINGENCODERS_INCLUDE_DIRS})

# Look for gmock/gtest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

find_package(gflags REQUIRED)
include_directories(${GFLAGS_INCLUDE_DIRS})

find_library(GMOCK_LIBRARY NAMES gmock)
if(NOT GMOCK_LIBRARY)
    message(FATAL_ERROR "GMock library not found")
endif(NOT GMOCK_LIBRARY)

find_library(GLOG_LIBRARY NAMES glog)
if(NOT GLOG_LIBRARY)
    message(FATAL_ERROR "GLog library not found")
endif(NOT GLOG_LIBRARY)

# Turn on C++11 in a way portable across CMake versions.
include(../cmake/Cxx11.cmake)
use_cxx11()

# Common compile flags.
include(../cmake/CompileFlags.cmake)

set(TARGET ndash)
list(APPEND NDASH_HEADERS
        src/chunk/adaptive_evaluator.h
        src/chunk/base_media_chunk.h
        src/chunk/chunk.h
        src/chunk/chunk_extractor_wrapper.h
        src/chunk/chunk_operation_holder.h
        src/chunk/chunk_sample_source.h
        src/chunk/chunk_sample_source_event_listener.h
        src/chunk/chunk_source.h
        src/chunk/container_media_chunk.h
        src/chunk/demo_evaluator.h
        src/chunk/fixed_evaluator.h
        src/chunk/format_evaluator.h
        src/chunk/initialization_chunk.h
        src/chunk/media_chunk.h
        src/chunk/single_sample_media_chunk.h
        src/chunk/single_track_output.h
        src/crypto_info.h
        src/dash/dash_chunk_source.h
        src/dash/dash_track_selector.h
        src/dash/dash_track_selector_output.h
        src/dash/dash_wrapping_segment_index.h
        src/dash/exposed_track.h
        src/dash/period_holder.h
        src/dash/representation_holder.h
        src/dash_thread.h
        src/drm/drm_init_data.h
        src/drm/drm_session_manager.h
        src/drm/license_fetcher.h
        src/drm/scheme_init_data.h
        src/extractor/chunk_index.h
        src/extractor/default_track_output.h
        src/extractor/extractor.h
        src/extractor/extractor_input.h
        src/extractor/extractor_output.h
        src/extractor/indexed_track_output.h
        src/extractor/info_queue.h
        src/extractor/rawcc_parser_extractor.h
        src/extractor/rolling_sample_buffer.h
        src/extractor/seek_map.h
        src/extractor/stream_parser_extractor.h
        src/extractor/track_output.h
        src/extractor/unbuffered_extractor_input.h
        src/load_control.h
        src/manifest_fetcher.h
        src/media_clock.h
        src/media_format.h
        src/media_format_holder.h
        src/mpd/adaptation_set.h
        src/mpd/content_protection.h
        src/mpd/content_protections_builder.h
        src/mpd/dash_manifest_representation_parser.h
        src/mpd/dash_segment_index.h
        src/mpd/dash_single_segment_index.h
        src/mpd/descriptor_type.h
        src/mpd/media_presentation_description.h
        src/mpd/multi_segment_base.h
        src/mpd/multi_segment_representation.h
        src/mpd/period.h
        src/mpd/ranged_uri.h
        src/mpd/representation.h
        src/mpd/segment_base.h
        src/mpd/segment_list.h
        src/mpd/segment_template.h
        src/mpd/segment_timeline_element.h
        src/mpd/single_segment_base.h
        src/mpd/single_segment_representation.h
        src/mpd/url_template.h
        src/ndash.h
        src/playback_rate.h
        src/player_attributes.h
        src/qoe/qoe_manager.h
        src/sample_holder.h
        src/sample_source.h
        src/sample_source_reader.h
        src/sample_source_track_renderer.h
        src/time_range.h
        src/track_criteria.h
        src/track_renderer.h
        src/upstream/allocator.h
        src/upstream/bandwidth_meter.h
        src/upstream/constants.h
        src/upstream/curl_data_source.h
        src/upstream/data_source.h
        src/upstream/data_spec.h
        src/upstream/default_allocator.h
        src/upstream/default_bandwidth_meter.h
        src/upstream/http_data_source.h
        src/upstream/loader.h
        src/upstream/loader_thread.h
        src/upstream/transfer_listener.h
        src/upstream/uri.h
        src/upstream/uri_data_source.h
        src/util/averager.h
        src/util/const_unique_ptr_map_value_iterator.h
        src/util/format.h
        src/util/hex.h
        src/util/mime_types.h
        src/util/sliding_median.h
        src/util/time.h
        src/util/uri_util.h
        src/util/util.h
        src/util/uuid.h
)
list(APPEND NDASH_SOURCES
        src/chunk/adaptive_evaluator.cc
        src/chunk/base_media_chunk.cc
        src/chunk/chunk.cc
        src/chunk/chunk_extractor_wrapper.cc
        src/chunk/chunk_operation_holder.cc
        src/chunk/chunk_sample_source.cc
        src/chunk/container_media_chunk.cc
        src/chunk/demo_evaluator.cc
        src/chunk/fixed_evaluator.cc
        src/chunk/initialization_chunk.cc
        src/chunk/media_chunk.cc
        src/chunk/single_sample_media_chunk.cc
        src/crypto_info.cc
        src/dash/dash_chunk_source.cc
        src/dash/dash_wrapping_segment_index.cc
        src/dash/exposed_track.cc
        src/dash/period_holder.cc
        src/dash/representation_holder.cc
        src/dash_thread.cc
        src/drm/drm_init_data.cc
        src/drm/drm_session_manager.cc
        src/drm/license_fetcher.cc
        src/drm/scheme_init_data.cc
        src/extractor/chunk_index.cc
        src/extractor/default_track_output.cc
        src/extractor/extractor.cc
        src/extractor/info_queue.cc
        src/extractor/rawcc_parser_extractor.cc
        src/extractor/rolling_sample_buffer.cc
        src/extractor/seek_map.cc
        src/extractor/stream_parser_extractor.cc
        src/extractor/unbuffered_extractor_input.cc
        src/load_control.cc
        src/manifest_fetcher.cc
        src/media_format.cc
        src/mpd/adaptation_set.cc
        src/mpd/content_protection.cc
        src/mpd/content_protections_builder.cc
        src/mpd/dash_manifest_representation_parser.cc
        src/mpd/dash_single_segment_index.cc
        src/mpd/descriptor_type.cc
        src/mpd/media_presentation_description.cc
        src/mpd/multi_segment_base.cc
        src/mpd/multi_segment_representation.cc
        src/mpd/period.cc
        src/mpd/ranged_uri.cc
        src/mpd/representation.cc
        src/mpd/segment_base.cc
        src/mpd/segment_list.cc
        src/mpd/segment_template.cc
        src/mpd/single_segment_base.cc
        src/mpd/single_segment_representation.cc
        src/mpd/url_template.cc
        src/ndash.cc
        src/playback_rate.cc
        src/qoe/qoe_manager.cc
        src/sample_holder.cc
        src/sample_source_track_renderer.cc
        src/time_range.cc
        src/track_criteria.cc
        src/track_renderer.cc
        src/upstream/bandwidth_meter.cc
        src/upstream/curl_data_source.cc
        src/upstream/data_spec.cc
        src/upstream/default_allocator.cc
        src/upstream/default_bandwidth_meter.cc
        src/upstream/loader_thread.cc
        src/upstream/uri.cc
        src/util/format.cc
        src/util/mime_types.cc
        src/util/sliding_median.cc
        src/util/uri_util.cc
        src/util/util.cc
        src/util/uuid.cc
)
set_source_files_properties(${NDASH_HEADERS} PROPERTIES HEADER_FILE_ONLY "TRUE")
add_library(ndash SHARED ${NDASH_SOURCES} ${NDASH_HEADERS})
add_dependencies(ndash
        chromium-base
        chromium-mp4
)
target_link_libraries(ndash
        chromium-base
        chromium-mp4
)

install(TARGETS ndash LIBRARY DESTINATION lib)
install(FILES src/ndash.h src/ndash.h DESTINATION include)

# Enable the possibility of executing unit tests.
enable_testing()

set(TARGET ndash_unittests)
list(APPEND NDASH_UNITTESTS_SOURCES
        src/chunk/adaptive_evaluator_unittest.cc
        src/chunk/base_media_chunk_mock.cc
        src/chunk/base_media_chunk_mock.h
        src/chunk/base_media_chunk_unittest.cc
        src/chunk/chunk_mock.cc
        src/chunk/chunk_mock.h
        src/chunk/chunk_operation_holder_unittest.cc
        src/chunk/chunk_sample_source_unittest.cc
        src/chunk/chunk_source_mock.cc
        src/chunk/chunk_source_mock.h
        src/chunk/chunk_unittest.cc
        src/chunk/container_media_chunk_unittest.cc
        src/chunk/demo_evaluator_unittest.cc
        src/chunk/fixed_evaluator_unittest.cc
        src/chunk/format_evaluator_mock.cc
        src/chunk/format_evaluator_mock.h
        src/chunk/format_evaluator_unittest.cc
        src/chunk/initialization_chunk_mock.cc
        src/chunk/initialization_chunk_mock.h
        src/chunk/initialization_chunk_unittest.cc
        src/chunk/media_chunk_mock.cc
        src/chunk/media_chunk_mock.h
        src/chunk/media_chunk_unittest.cc
        src/chunk/single_sample_media_chunk_unittest.cc
        src/chunk/single_track_output_mock.cc
        src/chunk/single_track_output_mock.h
        src/chunk/single_track_output_unittest.cc
        src/crypto_info_unittest.cc
        src/dash/dash_chunk_source_unittest.cc
        src/dash/dash_track_selector_mock.cc
        src/dash/dash_track_selector_mock.h
        src/dash/dash_track_selector_output_mock.cc
        src/dash/dash_track_selector_output_mock.h
        src/dash/dash_track_selector_output_unittest.cc
        src/dash/dash_track_selector_unittest.cc
        src/dash/dash_wrapping_segment_index_unittest.cc
        src/dash/period_holder_unittest.cc
        src/dash/representation_holder_unittest.cc
        src/drm/drm_init_data_mock.cc
        src/drm/drm_init_data_mock.h
        src/drm/drm_init_data_unittest.cc
        src/drm/drm_session_manager_mock.cc
        src/drm/drm_session_manager_mock.h
        src/drm/drm_session_manager_unittest.cc
        src/drm/license_fetcher_unittest.cc
        src/drm/scheme_init_data_unittest.cc
        src/extractor/chunk_index_unittest.cc
        src/extractor/chunk_index_unittest.h
        src/extractor/default_track_output_unittest.cc
        src/extractor/extractor_input_mock.cc
        src/extractor/extractor_input_mock.h
        src/extractor/extractor_input_unittest.cc
        src/extractor/extractor_mock.cc
        src/extractor/extractor_mock.h
        src/extractor/extractor_output_mock.cc
        src/extractor/extractor_output_mock.h
        src/extractor/extractor_output_unittest.cc
        src/extractor/indexed_track_output_mock.cc
        src/extractor/indexed_track_output_mock.h
        src/extractor/indexed_track_output_unittest.cc
        src/extractor/info_queue_unittest.cc
        src/extractor/rawcc_parser_extractor_unittest.cc
        src/extractor/rolling_sample_buffer_unittest.cc
        src/extractor/seek_map_mock.cc
        src/extractor/seek_map_mock.h
        src/extractor/seek_map_unittest.cc
        src/extractor/stream_parser_extractor_unittest.cc
        src/extractor/track_output_mock.cc
        src/extractor/track_output_mock.h
        src/extractor/track_output_unittest.cc
        src/load_control_unittest.cc
        src/manifest_fetcher_unittest.cc
        src/media_format_unittest.cc
        src/mpd/adaptation_set_unittest.cc
        src/mpd/content_protection_unittest.cc
        src/mpd/content_protections_builder_unittest.cc
        src/mpd/dash_manifest_representation_parser_unittest.cc
        src/mpd/dash_single_segment_index_unittest.cc
        src/mpd/descriptor_type_unittest.cc
        src/mpd/media_presentation_description_unittest.cc
        src/mpd/mpd_unittest_helper.cc
        src/mpd/mpd_unittest_helper.h
        src/mpd/period_unittest.cc
        src/mpd/ranged_uri_unittest.cc
        src/mpd/representation_unittest.cc
        src/mpd/segment_list_unittest.cc
        src/mpd/segment_template_unittest.cc
        src/mpd/single_segment_base_unittest.cc
        src/mpd/single_segment_representation_unittest.cc
        src/mpd/url_template_unittest.cc
        src/playback_rate_unittest.cc
        src/sample_holder_unittest.cc
        src/sample_source_mock.cc
        src/sample_source_mock.h
        src/sample_source_reader_mock.cc
        src/sample_source_reader_mock.h
        src/sample_source_track_renderer_unittest.cc
        src/time_range_mock.cc
        src/time_range_mock.h
        src/time_range_unittest.cc
        src/test/stream_parser_mock.cc
        src/test/stream_parser_mock.h
        src/test/test_data.h
        src/test/test_data.cc
        src/upstream/allocator_mock.cc
        src/upstream/allocator_mock.h
        src/upstream/allocator_unittest.cc
        src/upstream/bandwidth_meter_mock.cc
        src/upstream/bandwidth_meter_mock.h
        src/upstream/bandwidth_meter_unittest.cc
        src/upstream/curl_data_source_unittest.cc
        src/upstream/data_source_mock.cc
        src/upstream/data_source_mock.h
        src/upstream/data_source_unittest.cc
        src/upstream/data_spec_unittest.cc
        src/upstream/default_allocator_unittest.cc
        src/upstream/default_bandwidth_meter_unittest.cc
        src/upstream/http_data_source_mock.cc
        src/upstream/http_data_source_mock.h
        src/upstream/http_data_source_unittest.cc
        src/upstream/loader_mock.cc
        src/upstream/loader_mock.h
        src/upstream/loader_thread_unittest.cc
        src/upstream/loader_unittest.cc
        src/upstream/transfer_listener_mock.cc
        src/upstream/transfer_listener_mock.h
        src/upstream/transfer_listener_unittest.cc
        src/upstream/uri_unittest.cc
        src/util/averager_mock.cc
        src/util/averager_mock.h
        src/util/averager_unittest.cc
        src/util/format_unittest.cc
        src/util/mime_types_unittest.cc
        src/util/run_all_unittests.cc
        src/util/sliding_median_unittest.cc
        src/util/uri_util_unittest.cc
        src/util/util_unittest.cc
        src/util/uuid_unittest.cc
)
add_executable(ndash_unittests ${NDASH_UNITTESTS_SOURCES})
add_dependencies(ndash_unittests
        chromium-base
        chromium-mp4
        chromium-base-test
        ndash)
target_link_libraries(ndash_unittests
        ndash
        chromium-base-test
        pthread
        ${XML2_LIBRARIES}
        ${LIBEVENT_LIBRARIES}
        ${LIBCURL_LIBRARIES}
        ${STRINGENCODERS_LIBRARIES}
        ${GTEST_LIBRARIES}
        ${GMOCK_LIBRARY}
        ${GFLAGS_LIBRARIES}
        ${GLOG_LIBRARY}
)
add_test(ndash_unittests ndash_unittests)
