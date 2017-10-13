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

project(chromium_base)
cmake_minimum_required(VERSION 2.8)

# Include paths relative to src directory.
include_directories(src)

# Turn on C++11 in a way portable across CMake versions.
include(../../../cmake/Cxx11.cmake)
use_cxx11()

# Common compile flags.
include(../../../cmake/CompileFlags.cmake)

set(TARGET chromium-base)
list(APPEND BASE_SOURCES
        src/base/allocator/allocator_extension.cc
        src/base/allocator/allocator_shim.cc
        src/base/allocator/allocator_shim_default_dispatch_to_glibc.cc
        src/base/at_exit.cc
        src/base/base64.cc
        src/base/base64url.cc
        src/base/base_switches.cc
        src/base/bind_helpers.cc
        src/base/build_time.cc
        src/base/callback_helpers.cc
        src/base/callback_internal.cc
        src/base/command_line.cc
        src/base/cpu.cc
        src/base/debug/alias.cc
        src/base/debug/debugger.cc
        src/base/debug/debugger_posix.cc
        src/base/debug/stack_trace.cc
        src/base/debug/stack_trace_posix.cc
        src/base/debug/task_annotator.cc
        src/base/environment.cc
        src/base/files/file.cc
        src/base/files/file_enumerator.cc
        src/base/files/file_enumerator_posix.cc
        src/base/files/file_path.cc
        src/base/files/file_path_constants.cc
        src/base/files/file_path_watcher.cc
        src/base/files/file_path_watcher_linux.cc
        src/base/files/file_posix.cc
        src/base/files/file_tracing.cc
        src/base/files/file_util.cc
        src/base/files/file_util_linux.cc
        src/base/files/file_util_posix.cc
        src/base/files/important_file_writer.cc
        src/base/files/memory_mapped_file.cc
        src/base/files/memory_mapped_file_posix.cc
        src/base/files/scoped_file.cc
        src/base/files/scoped_temp_dir.cc
        src/base/guid.cc
        src/base/hash.cc
        src/base/json/json_file_value_serializer.cc
        src/base/json/json_parser.cc
        src/base/json/json_reader.cc
        src/base/json/json_string_value_serializer.cc
        src/base/json/json_value_converter.cc
        src/base/json/json_writer.cc
        src/base/json/string_escape.cc
        src/base/lazy_instance.cc
        src/base/location.cc
        src/base/logging.cc
        src/base/md5.cc
        src/base/memory/aligned_memory.cc
        src/base/memory/ref_counted.cc
        src/base/memory/ref_counted_memory.cc
        src/base/memory/shared_memory_posix.cc
        src/base/memory/singleton.cc
        src/base/memory/weak_ptr.cc
        src/base/message_loop/incoming_task_queue.cc
        src/base/message_loop/message_loop.cc
        src/base/message_loop/message_loop_task_runner.cc
        src/base/message_loop/message_pump.cc
        src/base/message_loop/message_pump_default.cc
        src/base/message_loop/message_pump_libevent.cc
        src/base/metrics/bucket_ranges.cc
        src/base/metrics/field_trial.cc
        src/base/metrics/metrics_hashes.cc
        src/base/metrics/histogram_base.cc
        src/base/metrics/histogram.cc
        src/base/metrics/histogram_samples.cc
        src/base/metrics/histogram_snapshot_manager.cc
        src/base/metrics/persistent_histogram_allocator.cc
        src/base/metrics/persistent_memory_allocator.cc
        src/base/metrics/persistent_sample_map.cc
        src/base/metrics/sample_map.cc
        src/base/metrics/sample_vector.cc
        src/base/metrics/sparse_histogram.cc
        src/base/metrics/statistics_recorder.cc
        src/base/pending_task.cc
        src/base/pickle.cc
        src/base/posix/file_descriptor_shuffle.cc
        src/base/posix/global_descriptors.cc
        src/base/posix/safe_strerror.cc
        src/base/posix/unix_domain_socket_linux.cc
        src/base/process/internal_linux.cc
        src/base/process/kill.cc
        src/base/process/kill_posix.cc
        src/base/process/launch.cc
        src/base/process/launch_posix.cc
        src/base/process/process_handle_linux.cc
        src/base/process/process_iterator.cc
        src/base/process/process_iterator_linux.cc
        src/base/process/process_handle_posix.cc
        src/base/process/process_metrics.cc
        src/base/process/process_metrics_linux.cc
        src/base/process/process_metrics_posix.cc
        src/base/process/process_posix.cc
        src/base/profiler/scoped_profile.cc
        src/base/profiler/scoped_tracker.cc
        src/base/profiler/tracked_time.cc
        src/base/rand_util.cc
        src/base/rand_util_posix.cc
        src/base/run_loop.cc
        src/base/sequence_checker_impl.cc
        src/base/sequenced_task_runner.cc
        src/base/sha1_portable.cc
        src/base/strings/pattern.cc
        src/base/strings/safe_sprintf.cc
        src/base/strings/string16.cc
        src/base/strings/string_number_conversions.cc
        src/base/strings/string_piece.cc
        src/base/strings/stringprintf.cc
        src/base/strings/string_split.cc
        src/base/strings/string_util.cc
        src/base/strings/string_util_constants.cc
        src/base/strings/sys_string_conversions_posix.cc
        src/base/strings/utf_string_conversions.cc
        src/base/strings/utf_string_conversion_utils.cc
        src/base/synchronization/cancellation_flag.cc
        src/base/synchronization/condition_variable_posix.cc
        src/base/synchronization/lock.cc
        src/base/synchronization/lock_impl_posix.cc
        src/base/synchronization/waitable_event_posix.cc
        src/base/synchronization/waitable_event_watcher_posix.cc
        src/base/sync_socket_posix.cc
        src/base/sys_info.cc
        src/base/sys_info_chromeos.cc
        src/base/sys_info_linux.cc
        src/base/sys_info_posix.cc
        src/base/task_runner.cc
        src/base/task/cancelable_task_tracker.cc
        src/base/task_scheduler/scheduler_lock_impl.cc
        src/base/task_scheduler/sequence.cc
        src/base/task_scheduler/sequence_sort_key.cc
        src/base/task_scheduler/task.cc
        src/base/task_scheduler/task_traits.cc
        src/base/third_party/icu/icu_utf.cc
        src/base/third_party/nspr/prtime.cc
        src/base/threading/non_thread_safe_impl.cc
        src/base/threading/platform_thread_internal_posix.cc
        src/base/threading/platform_thread_linux.cc
        src/base/threading/platform_thread_posix.cc
        src/base/threading/post_task_and_reply_impl.cc
        src/base/threading/sequenced_task_runner_handle.cc
        src/base/threading/sequenced_worker_pool.cc
        src/base/threading/simple_thread.cc
        src/base/threading/thread.cc
        src/base/threading/thread_checker_impl.cc
        src/base/threading/thread_collision_warner.cc
        src/base/threading/thread_id_name_manager.cc
        src/base/threading/thread_local_posix.cc
        src/base/threading/thread_local_storage.cc
        src/base/threading/thread_local_storage_posix.cc
        src/base/threading/thread_restrictions.cc
        src/base/threading/thread_task_runner_handle.cc
        src/base/threading/worker_pool.cc
        src/base/threading/worker_pool_posix.cc
        src/base/timer/elapsed_timer.cc
        src/base/timer/timer.cc
        src/base/time/clock.cc
        src/base/time/default_clock.cc
        src/base/time/default_tick_clock.cc
        src/base/time/tick_clock.cc
        src/base/time/time.cc
        src/base/time/time_posix.cc
        src/base/trace_event/malloc_dump_provider.cc
        src/base/trace_event/heap_profiler_allocation_context.cc
        src/base/trace_event/heap_profiler_allocation_context_tracker.cc
        src/base/trace_event/heap_profiler_allocation_register.cc
        src/base/trace_event/heap_profiler_allocation_register_posix.cc
        src/base/trace_event/heap_profiler_heap_dump_writer.cc
        src/base/trace_event/heap_profiler_stack_frame_deduplicator.cc
        src/base/trace_event/heap_profiler_type_name_deduplicator.cc
        src/base/trace_event/memory_allocator_dump.cc
        src/base/trace_event/memory_allocator_dump_guid.cc
        src/base/trace_event/memory_dump_manager.cc
        src/base/trace_event/memory_dump_request_args.cc
        src/base/trace_event/memory_dump_session_state.cc
        src/base/trace_event/process_memory_dump.cc
        src/base/trace_event/process_memory_maps.cc
        src/base/trace_event/process_memory_totals.cc
        src/base/trace_event/trace_buffer.cc
        src/base/trace_event/trace_config.cc
        src/base/trace_event/trace_event_argument.cc
        src/base/trace_event/trace_event_impl.cc
        src/base/trace_event/trace_event_memory_overhead.cc
        src/base/trace_event/trace_event_synthetic_delay.cc
        src/base/trace_event/trace_log.cc
        src/base/trace_event/trace_log_constants.cc
        src/base/trace_event/trace_sampling_thread.cc
        src/base/tracked_objects.cc
        src/base/tracking_info.cc
        src/base/values.cc
        src/base/version.cc
        src/base/vlog.cc
        )
add_library(chromium-base ${BASE_SOURCES} ${CORE_HEADERS})

set(TARGET chromium-base-test)
list(APPEND BASE_TEST_SOURCES
        src/base/test/test_pending_task.h
        src/base/test/multiprocess_test.h
        src/base/test/multiprocess_test_android.cc
        src/base/test/test_simple_task_runner.cc
        src/base/test/test_timeouts.cc
        src/base/test/test_simple_task_runner.h
        src/base/test/test_switches.h
        src/base/test/simple_test_tick_clock.h
        src/base/test/trace_event_analyzer.cc
        src/base/test/test_switches.cc
        src/base/test/sequenced_worker_pool_owner.h
        src/base/test/scoped_locale.h
        src/base/test/opaque_ref_counted.cc
        src/base/test/trace_event_analyzer_unittest.cc
        src/base/test/multiprocess_test.cc
        src/base/test/sequenced_worker_pool_owner.cc
        src/base/test/scoped_locale.cc
        src/base/test/test_timeouts.h
        src/base/test/test_file_util.h
        src/base/test/test_io_thread.h
        src/base/test/test_file_util.cc
        src/base/test/trace_event_analyzer.h
        src/base/test/simple_test_tick_clock.cc
        src/base/test/test_io_thread.cc
        src/base/test/test_file_util_posix.cc
        src/base/test/test_pending_task.cc
        src/base/test/opaque_ref_counted.h
        src/base/test/simple_test_clock.cc
        src/base/test/test_file_util_linux.cc
        src/base/test/simple_test_clock.h)
add_library(chromium-base-test ${BASE_TEST_SOURCES})
add_dependencies(chromium-base-test chromium-base libgtest libgmock)
