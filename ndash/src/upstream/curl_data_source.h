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

#ifndef NDASH_UPSTREAM_CURL_DATA_SOURCE_H_
#define NDASH_UPSTREAM_CURL_DATA_SOURCE_H_

#include "upstream/http_data_source.h"

#include <curl/curl.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "upstream/transfer_listener.h"

namespace ndash {
namespace upstream {

class CurlDataSource : public HttpDataSourceInterface {
 public:
  // The maximum size of the internal buffer. This is also the maximum allowed
  // size for ReadAllToString(). When not using ReadAllToString(), as the
  // consumer reads from the start of the buffer, that space is made available
  // for the CURL thread to append more data. The constructor can override this
  // default size.
  //
  // The buffer should be sized approximately as large as the largest chunk
  // size expected so that the bandwidth meter measures network capability and
  // not CPU processing ability. With this default, we assume that the CPU is
  // powerful enough to process all of the available bitrates: if this is not
  // true, then shrinking the buffer will ensure that lower bitrates are
  // selected.
  //
  // This limit exists to ensure that if there is a manifest with a really
  // large chunk size (regardless of if it's legitimate or in error) then the
  // system will not run out of memory.
  static constexpr size_t kDefaultMaxBufLength = 10 * 1024 * 1024;  // 10MiB

  // content_type: a MIME type, used to name the CURL thread
  // listener: gets called when transfers start/end (nullptr if none)
  // use_global_lock: serialize this DataSource with others that use global lock
  // max_buffer_size: The maximum internal buffer size before CURL thread is
  //                  blocked
  CurlDataSource(const std::string& content_type,
                 TransferListenerInterface* listener = nullptr,
                 bool use_global_lock = false,
                 size_t max_buffer_size = kDefaultMaxBufLength);
  ~CurlDataSource() override;

  // DataSourceInterface
  // Opens and fetches the entire contents, storing it in a local buffer
  ssize_t Open(const DataSpec& dataSpec,
               const base::CancellationFlag* cancel = nullptr) override;
  // Clears everything out, making it ready for the next request
  void Close() override;
  // Just reads from the local buffer
  ssize_t Read(void* out_buffer, size_t read_length) override;

  // UriDataSourceInterface
  const char* GetUri() const override;

  // HttpDataSourceInterface
  void SetRequestProperty(const std::string& name,
                          const std::string& value) override;
  void ClearRequestProperty(const std::string& name) override;
  void ClearAllRequestProperties() override;
  const std::multimap<std::string, std::string>* GetResponseHeaders()
      const override;
  int GetResponseCode() const override;
  HttpDataSourceError GetHttpError() const override;
  // If max_length is 0, then uses max_buffer_size passed at construction time.
  std::string ReadAllToString(size_t max_length = 0) override;

 private:
  friend class CurlDataSourceTest;

  const bool use_global_lock_;
  const size_t max_buffer_size_;

  base::Lock* global_active_lock_;
  bool* global_active_;
  base::ConditionVariable* global_active_done_;

  // No lock required: set by constructor
  TransferListenerInterface* const listener_;

  // No lock required: request data. Only modified when Curl thread is idle.
  bool open_ = false;    // Open() was called, so Close() needs to reset
  bool active_ = false;  // Actually fired off task on curl thread
  std::map<std::string, std::string> request_properties_;
  std::unique_ptr<struct curl_slist, void (*)(struct curl_slist*)>
      curl_request_headers_;
  const base::CancellationFlag* cancel_ = nullptr;
  bool is_http_ = false;
  bool is_range_request_ = false;
  std::string uri_;

  // Protected by buffer_lock_ (Curl thread running a task) or
  // curl_done_.IsSignaled() (implies Curl thread not running a task)
  base::Lock buffer_lock_;
  base::ConditionVariable reader_;  // For waiting on reader to be ready
  base::ConditionVariable writer_;  // For waiting on write completion
  bool load_error_ = false;         // Set by writer on any error
  bool eof_ = false;                // Set by writer when complete
  bool headers_seen_ = false;
  bool end_of_headers_ = true;
  bool want_full_buffer_ = false;
  // |bytes_buffered_| tracks the buffer's memory use: it is not reduced until
  // an item is popped from buffers_ (so when buffer_head_pos_ increments, this
  // does not change)
  size_t bytes_buffered_ = 0;
  // This tracks how much of buffers_.head() has been read by the consumer
  // already, to avoid re-reading the same data if an item in buffers_ was not
  // fully consumed by a previous Read() call.
  size_t buffer_head_pos_ = 0;
  std::queue<std::string> buffers_;

  base::WaitableEvent curl_done_;
  base::WaitableEvent headers_done_;

  // No base::Lock required: header data
  // - Initially, the Curl thread is not running a task so Loader thread can
  //   safely write.
  // - After Curl thread has a task posted, access from Loader thread waits
  //   until headers_done_.IsSignaled(). The Curl thread is allowed to
  //   read/write until that time, and never after. The Loader thread can read
  //   from these after Open() returns, as that implies
  //   headers_done_.IsSignaled()
  ssize_t tentative_length_ = LENGTH_UNBOUNDED;
  std::multimap<std::string, std::string> response_headers_;

  // No lock required: only accessed on loader thread
  size_t bytes_read_ = 0;

  // No base::Lock required: curl state
  // - Initially, the Curl thread is not running a task so Loader thread can
  //   safely write.
  // - After Curl thread has a task posted, access from Loader thread waits
  //   until curl_done_.IsSignaled(). The Curl thread is allowed to
  //   read/write until that time, and never after.
  std::unique_ptr<CURL, void (*)(CURL*)> easy_;
  char curl_error_buf_[CURL_ERROR_SIZE] = {0};
  HttpDataSourceError http_error_ = HTTP_OK;

  base::Thread curl_thread_;

  base::TimeTicks load_start_time_;

  base::TimeTicks loader_handoff_time_;
  base::TimeDelta loader_processing_time_;
  base::TimeDelta loader_waiting_time_;
  base::ThreadTicks loader_thread_start_;

  base::TimeTicks curl_handoff_time_;
  base::TimeDelta curl_first_header_time_;
  base::TimeDelta curl_processing_time_;
  base::TimeDelta curl_waiting_time_;
  base::TimeDelta curl_finish_time_;
  base::TimeDelta curl_cpu_time_;

  // These must be called with buffer_lock_ held
  // This returns the number of the bytes waiting to be processed by Read(). It
  // is not the opposite of GetBufferFree() because this subtracts the portion
  // of buffers_.head() that has been read already, whereas GetBufferFree()
  // tracks actual memory usage.
  size_t GetReadable() const {
    buffer_lock_.AssertAcquired();
    return bytes_buffered_ - buffer_head_pos_;
  }
  // This does not look at buffer_head_pos_ because it refers to the memory
  // actually consumed by the buffer; it changes only when buffers_ has an item
  // pushed or popped.
  size_t GetBufferFree() const {
    buffer_lock_.AssertAcquired();
    return max_buffer_size_ - bytes_buffered_;
  }

  // These run on curl_thread_
  static size_t CurlWriteCallback(const void* ptr,
                                  size_t size,
                                  size_t nmemb,
                                  void* userdata);
  static size_t CurlHeaderCallback(const void* ptr,
                                   size_t size,
                                   size_t nmemb,
                                   void* userdata);
  void CurlPerform();

  // CURL options and info uses varargs to take multiple types. Using a
  // template wrapper for error handling makes is more C++ friendly than trying
  // to pass varargs through, and involves less repetition than using an
  // overload.
  template <typename T>
  bool SetCurlOption(bool continue_on_error,
                     CURLoption option,
                     T param,
                     const char* option_description);
  template <typename T>
  bool GetCurlInfo(bool continue_on_error,
                   CURLINFO option,
                   T* param,
                   const char* info_description) const;

  // Returns an error code corresponding to DataSourceInterface; sets up
  // http_error_ for the more specific error. Used as a helper for Open()
  ssize_t DataSourceError(HttpDataSourceError http_error);

  bool BuildRequestHeaders();

  // Runs on curl_thread_
  bool ProcessResponseHeader(base::StringPiece header_line);
  bool ProcessHeadersComplete();
  bool ProcessBodyData(const void* buffer, size_t size);
  bool CheckCancel(const char* where);

  void BeforeLoaderWait();
  void AfterLoaderWait();
  void BeforeCurlWait();
  void AfterCurlWait();
  void AfterCurlFinishWait();

  DISALLOW_COPY_AND_ASSIGN(CurlDataSource);
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_CURL_DATA_SOURCE_H_
