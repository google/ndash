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

#ifndef NDASH_CHUNK_SAMPLE_SOURCE_EVENT_LISTENER_H_
#define NDASH_CHUNK_SAMPLE_SOURCE_EVENT_LISTENER_H_

#include <cstdint>

#include "base/time/time.h"
#include "chunk/chunk_source.h"
#include "util/format.h"

namespace ndash {
namespace chunk {

// Interface for callbacks to be notified of chunk based SampleSource events.
class ChunkSampleSourceEventListenerInterface {
 public:
  ChunkSampleSourceEventListenerInterface() {}
  virtual ~ChunkSampleSourceEventListenerInterface() {}

  // Invoked when an upstream load is started.
  //
  // source_id The id of the reporting SampleSource.
  // length The length of the data being loaded in bytes, or kLengthUnbounded
  //   if the length of the data is not known in advance.
  // type The type of the data being loaded.
  // trigger The reason for the data being loaded.
  // format The particular format to which this data corresponds, or null if the
  //   data being loaded does not correspond to a format.
  // media_start_time_ms The media time of the start of the data being loaded,
  // or
  //   -1 if this load is for initialization data.
  // media_end_time_ms The media time of the end of the data being loaded, or -1
  // if
  //   this load is for initialization data.
  virtual void OnLoadStarted(int32_t source_id,
                             int64_t length,
                             int32_t type,
                             int32_t trigger,
                             const util::Format* format,
                             int64_t media_start_time_ms,
                             int64_t media_end_time_ms) = 0;

  // Invoked when the current load operation completes.
  //
  // source_id The id of the reporting SampleSource.
  // bytes_loaded The number of bytes that were loaded.
  // type The type of the loaded data.
  // trigger The reason for the data being loaded.
  // format The particular format to which this data corresponds, or null if
  //   the loaded data does not correspond to a format.
  // media_start_time_ms The media time of the start of the loaded data, or -1
  //   if this load was for initialization data.
  // media_end_time_ms The media time of the end of the loaded data, or -1 if
  //   this load was for initialization data.
  // elapsed_real_time_ms timestamp of when the load finished.
  // load_duration_ms Amount of time taken to load the data.
  virtual void OnLoadCompleted(int32_t source_id,
                               int64_t bytes_loaded,
                               int32_t type,
                               int32_t trigger,
                               const util::Format* format,
                               int64_t media_start_time_ms,
                               int64_t media_end_time_ms,
                               base::TimeTicks elapsed_real_time,
                               base::TimeDelta load_duration) = 0;

  // Invoked when the current upstream load operation is canceled.
  //
  // source_id The id of the reporting SampleSource.
  // bytesLoaded The number of bytes that were loaded prior to the cancellation.
  virtual void OnLoadCanceled(int32_t source_id, int64_t bytesLoaded) = 0;

  // Invoked when an error occurs loading media data.
  //
  // source_id The id of the reporting SampleSource.
  // e The reason for the failure.
  virtual void OnLoadError(int32_t source_id, ChunkLoadErrorReason e) = 0;

  // Invoked when data is removed from the back of the buffer, typically so that
  // it can be
  // re-buffered using a different representation.
  //
  // source_id The id of the reporting SampleSource.
  // media_start_time_ms The media time of the start of the discarded data.
  // media_end_time_ms The media time of the end of the discarded data.
  virtual void OnUpstreamDiscarded(int32_t source_id,
                                   int64_t media_start_time_ms,
                                   int64_t media_end_time_ms) = 0;

  // Invoked when the downstream format changes (i.e. when the format being
  // supplied to the caller of SampleSourceReader::ReadData changes).
  //
  // source_id The id of the reporting SampleSource.
  // format The format.
  // trigger The trigger specified in the corresponding upstream load, as
  // specified by the ChunkSource.
  // media_time_ms The media time at which the change occurred.
  virtual void OnDownstreamFormatChanged(int32_t source_id,
                                         const util::Format* format,
                                         int32_t trigger,
                                         int64_t media_time_ms) = 0;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_SAMPLE_SOURCE_EVENT_LISTENER_H_
