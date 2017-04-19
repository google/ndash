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

#ifndef NDASH_MEDIA_SAMPLE_SOURCE_READER_H_
#define NDASH_MEDIA_SAMPLE_SOURCE_READER_H_

#include <cstdint>
#include <limits>

#include "media_format_holder.h"
#include "sample_holder.h"
#include "track_criteria.h"

namespace ndash {

// An interface providing read access to a SampleSource.
class SampleSourceReaderInterface {
 public:
  enum ReadResult {
    // The end of stream has been reached.
    END_OF_STREAM = -1,
    // Neither a sample nor a format was read in full. This may be because
    // insufficient data is buffered upstream.
    NOTHING_READ = -2,
    // A sample was read.
    SAMPLE_READ = -3,
    // A format was read.
    FORMAT_READ = -4,
    // Returned from ReadDiscontinuity to indicate no discontinuity.
    NO_DISCONTINUITY = std::numeric_limits<int64_t>::min(),
  };

  SampleSourceReaderInterface(){};
  virtual ~SampleSourceReaderInterface(){};

  // If the source is currently having difficulty preparing or loading samples,
  // then this method returns false. Otherwise returns true.
  virtual bool CanContinueBuffering() = 0;

  // Prepares the source.
  // Preparation may require reading from the data source (e.g. to determine
  // the available tracks and formats). If insufficient data is available then
  // the call will return {@code false} rather than block. The method can be
  // called repeatedly until the return value indicates success.
  // position_us: The player's current playback position.
  // Returns true if the source was prepared, false otherwise.
  virtual bool Prepare(int64_t position_us) = 0;

  // Returns the duration of the source, or kUnknownTimeUs if unknown.
  //
  // This method should only be called after the source has been prepared.
  //
  // @return The duration of the source
  virtual int64_t GetDurationUs() = 0;

  // Enable the source. Format and sample data for the track selected by
  // |track_criteria| may then be read by ReadData().
  //
  // This method should only be called after the source has been prepared.
  //
  // track_criteria: The criteria used to select a track.
  // position_us: The player's current playback position.
  virtual void Enable(const TrackCriteria* track_criteria,
                      int64_t position_us) = 0;

  // Indicates to the source that it should still be buffering data.
  //
  // This method should only be called when a track is enabled.
  //
  // position_us: The current playback position.
  // Returns true if the track has available samples, or if the end of the
  // stream has been reached. False if more data needs to be buffered for
  // samples to become available.
  virtual bool ContinueBuffering(int64_t position_us) = 0;

  // Attempts to read a pending discontinuity from the source.
  //
  // This method should only be called when a track is enabled.
  //
  // Return if a discontinuity was read then the playback position after the
  // discontinuity. Else NO_DISCONTINUITY.
  virtual int64_t ReadDiscontinuity() = 0;

  // Attempts to read a sample or a new format from the source.
  //
  // This method will always return NOTHING_READ in the case that there's a
  // pending discontinuity to be read.
  //
  // position_us: The current playback position.
  // format_holder: A MediaFormatHolder object to populate in the case
  //   of a new format.
  // sample_holder: A SampleHolder object to populate in the case of a
  //   new sample. If the caller requires the sample data then it must ensure
  //   that SampleHolder::data_ references a valid output buffer.
  // Returns the result, which can be SAMPLE_READ, FORMAT_READ,
  //   NOTHING_READ or END_OF_STREAM.
  //
  virtual ReadResult ReadData(int64_t position_us,
                              MediaFormatHolder* formatHolder,
                              SampleHolder* sampleHolder) = 0;

  // Seeks to the specified time in microseconds.
  //
  // This method should only be called when a track is enabled.
  //
  // position_us The seek position in microseconds.
  //
  virtual void SeekToUs(int64_t position_us) = 0;

  // Returns an estimate of the position up to which data is buffered.
  //
  // This method should only be called when at least one track is enabled.
  //
  // Returns an estimate of the absolute position in microseconds up to which
  // data is buffered,
  //     or kEndOfTrackUs if data is buffered to the end of the stream,
  //     or kUnknownTimeUs if no estimate is available.
  //
  virtual int64_t GetBufferedPositionUs() = 0;

  // Disable the specified track. Disabling a track is an asynchronous
  // operation.
  //
  // This method should only be called when the specified track is enabled.
  //
  // disable_done_callback: An optional callback to be invoked once the reader
  // has been disabled.
  virtual void Disable(
      const base::Closure* disable_done_callback = nullptr) = 0;

  // Releases the SampleSourceReader.
  //
  // This method should be called when access to the SampleSource is no
  // longer required.
  virtual void Release() = 0;
};

}  // namespace ndash

#endif  // NDASH_MEDIA_SAMPLE_SOURCE_READER_H_
