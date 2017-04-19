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

#ifndef NDASH_UPSTREAM_DATA_SOURCE_H_
#define NDASH_UPSTREAM_DATA_SOURCE_H_

#include <cstdlib>

#include "base/synchronization/cancellation_flag.h"
#include "upstream/constants.h"

namespace ndash {
namespace upstream {

class DataSpec;

// A component that provides media data.
class DataSourceInterface {
 public:
  virtual ~DataSourceInterface() {}

  // Opens the DataSource to read the specified data. Only one DataSpec can be
  // open at a time (call Close() before opening another). If Open() returns
  // failure (or is canceled), Close() is still required before calling Open()
  // again.
  //
  // Returns the number of bytes that can be read from the opened source. For
  // unbounded requests (where data_spec.length() equals LENGTH_UNBOUNDED) this
  // value is the resolved length of the request, or LENGTH_UNBOUNDED if the
  // length is still unresolved. For all other requests, the value returned
  // will be equal to the request's data_spec.length().
  //
  // If |cancel| is not null, then the underlying implementation should check
  // cancel->IsSet() periodically and abort in a clean fashion as soon as
  // reasonably possible. There is no guarantee that a cancel request is
  // actually honored.
  //
  // This call may block while the request is made.
  //
  // Upon failure (or a cancel that results in an early return), returns
  // RESULT_IO_ERROR
  virtual ssize_t Open(const DataSpec& data_spec,
                       const base::CancellationFlag* cancel = nullptr) = 0;

  // Closes the DataSource.
  virtual void Close() = 0;

  // Reads up to length bytes of data and stores them into buffer.
  // This method blocks until at least one byte of data can be read, the end of
  // the opened range is detected, or an error is returned.
  //
  // The buffer will not be null terminated and may contain nulls. This is
  // roughly equivalent to fread().
  //
  // Returns one of:
  // - The number of bytes read (>= 0); 0 is not an error.
  // - RESULT_END_OF_INPUT, if the end of the opened range is reached
  // - RESULT_IO_ERROR, if there was an error
  virtual ssize_t Read(void* buffer, size_t read_length) = 0;

 protected:
  DataSourceInterface() {}
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_DATA_SOURCE_H_
