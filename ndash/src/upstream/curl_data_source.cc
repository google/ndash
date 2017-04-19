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

#include "upstream/curl_data_source.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <unistd.h>

#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "upstream/constants.h"
#include "upstream/data_spec.h"

namespace ndash {
namespace upstream {

namespace {
const char kHttpScheme[] = "http:";
const char kHttpsScheme[] = "https:";
}  // namespace

// The maximum size of the buffer before further writes will block.
constexpr size_t CurlDataSource::kDefaultMaxBufLength;

CurlDataSource::CurlDataSource(const std::string& content_type,
                               TransferListenerInterface* listener,
                               bool use_global_lock,
                               size_t max_buffer_size)
    : use_global_lock_(use_global_lock),
      max_buffer_size_(max_buffer_size),
      listener_(listener),
      request_properties_(),
      curl_request_headers_(nullptr, curl_slist_free_all),
      buffer_lock_(),
      reader_(&buffer_lock_),
      writer_(&buffer_lock_),
      curl_done_(true, false),
      headers_done_(true, false),
      response_headers_(),
      easy_(curl_easy_init(), curl_easy_cleanup),
      curl_thread_(std::string("CURL:") + content_type) {
  // This is a hack to serialize HTTP requests, mostly so that video and audio
  // don't download concurrently, improving the amount of CPU available for
  // HTTPS decryption on a single stream, which is needed to make the bandwidth
  // meter produce reasonable results.
  //
  // DataSourceInterface normally restricts to 1 transfer at a time per
  // instance. This further restricts to 1 transfer at a time across all
  // CurlDataSources.
  // TODO(adewhurst): Remove this and come up with a better solution
  static base::Lock active_lock;
  static bool active = false;
  static base::ConditionVariable active_done(&active_lock);

  global_active_lock_ = &active_lock;
  global_active_ = &active;
  global_active_done_ = &active_done;

  curl_thread_.Start();
}

CurlDataSource::~CurlDataSource() {
  curl_thread_.Stop();
}

ssize_t CurlDataSource::Open(const DataSpec& data_spec,
                             const base::CancellationFlag* cancel) {
  if (open_) {
    // Can't have multiple simultaneous requests (DataSourceInterface spec)
    LOG(ERROR) << "Failed to open: request already in progress";
    return DataSourceError(HTTP_IO_ERROR);
  }

  if (use_global_lock_) {
    base::AutoLock auto_lock(*global_active_lock_);

    while (*global_active_) {
      global_active_done_->Wait();
    }

    *global_active_ = true;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  load_start_time_ = now;
  loader_handoff_time_ = now;
  loader_processing_time_ = base::TimeDelta();
  loader_waiting_time_ = base::TimeDelta();
  curl_cpu_time_ = base::TimeDelta();
  loader_thread_start_ = base::ThreadTicks::Now();

  open_ = true;
  SetCurlOption(true, CURLOPT_NOSIGNAL, 1, "disable signals");
  memset(curl_error_buf_, 0, sizeof(curl_error_buf_));
  // TODO(adewhurst): Useful error handling
  SetCurlOption(true, CURLOPT_ERRORBUFFER, curl_error_buf_, "error buffer");
  if (!SetCurlOption(false, CURLOPT_PROTOCOLS,
                     CURLPROTO_FILE | CURLPROTO_HTTP | CURLPROTO_HTTPS,
                     "allowed protocols")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (!SetCurlOption(false, CURLOPT_REDIR_PROTOCOLS,
                     CURLPROTO_HTTP | CURLPROTO_HTTPS, "redirect protocols")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  SetCurlOption(true, CURLOPT_AUTOREFERER, 1, "automatic referer");
  // TODO(adewhurst): Add option to restrict cross-protocol redirects?
  SetCurlOption(true, CURLOPT_FOLLOWLOCATION, 1, "follow location");
  // TODO(adewhurst): allow GZIP encoding
  // TODO(adewhurst): support POST
  if (!BuildRequestHeaders()) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (!SetCurlOption(false, CURLOPT_HTTPHEADER, curl_request_headers_.get(),
                     "request HTTP headers")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  // TODO(adewhurst): Set user-agent
  if (!SetCurlOption(false, CURLOPT_PRIVATE, this, "private data")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (!SetCurlOption(false, CURLOPT_WRITEFUNCTION, CurlWriteCallback,
                     "write callback")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (!SetCurlOption(false, CURLOPT_WRITEDATA, this, "write callback data")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (!SetCurlOption(false, CURLOPT_HEADERFUNCTION, CurlHeaderCallback,
                     "header callback")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (!SetCurlOption(false, CURLOPT_HEADERDATA, this, "header callback data")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  uri_ = data_spec.uri.uri();
  if (!SetCurlOption(false, CURLOPT_URL, uri_.c_str(), "URL")) {
    return DataSourceError(HTTP_IO_ERROR);
  }
  if (data_spec.post_body) {
    const std::string* post_body = data_spec.post_body.get();
    if (!SetCurlOption(false, CURLOPT_POSTFIELDS, post_body->data(),
                       "post body") ||
        !SetCurlOption(false, CURLOPT_POSTFIELDSIZE, post_body->size(),
                       "post body size")) {
      return DataSourceError(HTTP_IO_ERROR);
    }
  }

  // Set byte range to request
  if (!(data_spec.position == 0 && data_spec.length == LENGTH_UNBOUNDED)) {
    std::string range_request = std::to_string(data_spec.position) + "-";
    if (data_spec.length != LENGTH_UNBOUNDED) {
      range_request +=
          std::to_string(data_spec.position + data_spec.length - 1);
    }
    SetCurlOption(true, CURLOPT_RANGE, range_request.c_str(),
                  "byte range to request");

    VLOG(2) << "[CURL start] " << uri_ << " [" << range_request << "]";
    is_range_request_ = true;
  } else {
    VLOG(2) << "[CURL start] " << uri_ << " [all]";
    is_range_request_ = false;
  }

  is_http_ = base::StartsWith(uri_, kHttpScheme,
                              base::CompareCase::INSENSITIVE_ASCII) ||
             base::StartsWith(uri_, kHttpsScheme,
                              base::CompareCase::INSENSITIVE_ASCII);

  cancel_ = cancel;

  if (CheckCancel("before perform")) {
    return DataSourceError(HTTP_IO_ERROR);
  }

  CHECK(curl_thread_.IsRunning());
  active_ = true;
  curl_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CurlDataSource::CurlPerform, base::Unretained(this)));

  BeforeLoaderWait();
  headers_done_.Wait();
  AfterLoaderWait();

  if (CheckCancel("after headers")) {
    BeforeLoaderWait();
    curl_done_.Wait();
    AfterLoaderWait();
    return DataSourceError(HTTP_IO_ERROR);
  }

  {
    base::AutoLock buffer_autolock(buffer_lock_);
    if (load_error_) {
      // If there is a HTTP error, it should already be set
      return RESULT_IO_ERROR;
    }
  }

  if (data_spec.length != LENGTH_UNBOUNDED &&
      tentative_length_ != LENGTH_UNBOUNDED) {
    LOG_IF(WARNING, tentative_length_ != data_spec.length)
        << "Unexpected length " << tentative_length_ << "; requested "
        << data_spec.length;
  }

  return tentative_length_;
}

void CurlDataSource::CurlPerform() {
  curl_handoff_time_ = base::TimeTicks::Now();
  curl_first_header_time_ = base::TimeDelta();
  curl_processing_time_ = base::TimeDelta();
  curl_waiting_time_ = base::TimeDelta();
  curl_finish_time_ = base::TimeDelta();
  base::ThreadTicks curl_cpu_start = base::ThreadTicks::Now();

  CURLcode result = curl_easy_perform(easy_.get());

  if (headers_done_.IsSignaled()) {
    if (listener_) {
      listener_->OnTransferEnd();
    }
  } else {
    // Empty body (this comes up when getting a 204 response code)
    tentative_length_ = 0;
  }

  {
    base::AutoLock buffer_autolock(buffer_lock_);

    if (result != CURLE_OK) {
      LOG(INFO) << "Could not fetch: CURL result " << result
                << " / CURL error buffer " << curl_error_buf_;
      load_error_ = true;
    } else {
      eof_ = true;

      writer_.Signal();

      while (!buffers_.empty() && !load_error_) {
        BeforeCurlWait();
        reader_.Wait();
        AfterCurlFinishWait();
      }
    }
  }

  writer_.Broadcast();

  // TODO(adewhurst): Check content-type
  // TODO(adewhurst): Return content-length rather than buffer size (will
  //                  differ if GZIP enabled), unless libcurl decompresses

  BeforeCurlWait();  // Account for the remaining processing time
  curl_cpu_time_ = base::ThreadTicks::Now() - curl_cpu_start;

  VLOG(2) << "[CURL end] " << uri_;

  // Not usually needed, but this prevents deadlock in error cases
  headers_done_.Signal();

  // We're done
  curl_done_.Signal();
}

size_t CurlDataSource::CurlWriteCallback(const void* ptr,
                                         size_t size,
                                         size_t nmemb,
                                         void* userdata) {
  VLOG(10) << "CURL write: size " << size << ", nmemb " << nmemb;

  CurlDataSource* cds = reinterpret_cast<CurlDataSource*>(userdata);

  if (cds->CheckCancel("during body"))
    return 0;

  if (!cds->headers_done_.IsSignaled()) {
    // We're seeing body data, which implies that the headers are complete.
    // TODO(adewhurst): Check if this path triggers during a redirect, and
    //                  suppress it if so (we want ProcessHeadersComplete() to
    //                  populate tentative_length_ with the final request's
    //                  content length.

    bool headers_ok = cds->ProcessHeadersComplete();

    if (!headers_ok)
      return 0;
  }

  if (!cds->ProcessBodyData(ptr, size * nmemb))
    return 0;

  return nmemb;
}

size_t CurlDataSource::CurlHeaderCallback(const void* ptr,
                                          size_t size,
                                          size_t nmemb,
                                          void* userdata) {
  VLOG(10) << "CURL header: size " << size << ", nmemb " << nmemb;

  CurlDataSource* cds = reinterpret_cast<CurlDataSource*>(userdata);

  if (cds->CheckCancel("during headers"))
    return 0;

  const base::StringPiece header(reinterpret_cast<const char*>(ptr),
                                 size * nmemb);
  if (!cds->ProcessResponseHeader(
          base::TrimWhitespaceASCII(header, base::TRIM_ALL)))
    return 0;

  return nmemb;
}

void CurlDataSource::Close() {
  if (active_) {
    // Effectively trigger a cancel if needed
    {
      base::AutoLock buffer_autolock(buffer_lock_);
      load_error_ = true;
    }

    reader_.Broadcast();
    BeforeLoaderWait();
    curl_done_.Wait();
    AfterLoaderWait();
  }

  if (open_) {
    if (use_global_lock_) {
      base::AutoLock auto_lock(*global_active_lock_);
      *global_active_ = false;
      global_active_done_->Signal();
    }

    VLOG(2) << "[CURL time] Loader (processing=" << loader_processing_time_
            << ", waiting=" << loader_waiting_time_
            << ", total=" << (loader_processing_time_ + loader_waiting_time_)
            << ", cpu=" << (base::ThreadTicks::Now() - loader_thread_start_)
            << "), Curl (request=" << curl_first_header_time_
            << ", processing=" << curl_processing_time_
            << ", waiting=" << curl_waiting_time_
            << ", finish=" << curl_finish_time_ << ", total="
            << (curl_first_header_time_ + curl_processing_time_ +
                curl_waiting_time_ + curl_finish_time_)
            << ", cpu=" << curl_cpu_time_
            << ") Open=" << (base::TimeTicks::Now() - load_start_time_) << " "
            << " bytes=" << bytes_read_ << " " << uri_;
  }

  open_ = false;
  active_ = false;
  cancel_ = nullptr;
  is_http_ = false;
  is_range_request_ = false;
  uri_.clear();
  curl_easy_reset(easy_.get());
  load_error_ = false;
  eof_ = false;
  headers_seen_ = false;
  end_of_headers_ = true;
  want_full_buffer_ = false;
  bytes_buffered_ = 0;
  buffer_head_pos_ = 0;
  while (!buffers_.empty())
    buffers_.pop();
  curl_done_.Reset();
  headers_done_.Reset();
  tentative_length_ = LENGTH_UNBOUNDED;
  response_headers_.clear();
  bytes_read_ = 0;
  http_error_ = HTTP_OK;
}

ssize_t CurlDataSource::Read(void* out_buffer, size_t read_length) {
  CHECK(out_buffer);

  if (read_length > std::numeric_limits<ssize_t>::max()) {
    read_length = std::numeric_limits<ssize_t>::max();
  }

  if (read_length == 0)
    return 0;

  size_t return_size = 0;

  {
    base::AutoLock buffer_autolock(buffer_lock_);

    while (!load_error_ && !eof_ && buffers_.empty()) {
      BeforeLoaderWait();
      writer_.Wait();
      AfterLoaderWait();
    }

    if (load_error_)
      return RESULT_IO_ERROR;
    if (buffers_.empty() && eof_)
      return RESULT_END_OF_INPUT;

    CHECK(!buffers_.empty());

    size_t bytes_to_copy = std::min(read_length, GetReadable());
    return_size = bytes_to_copy;

    char* out_char = reinterpret_cast<char*>(out_buffer);
    bool signal_reader = false;

    while (bytes_to_copy > 0) {
      const std::string& buffer_head = buffers_.front();
      DCHECK_LT(buffer_head_pos_, buffer_head.size());
      DCHECK_GE(bytes_buffered_, buffer_head.size());

      size_t buffer_head_remaining = buffer_head.size() - buffer_head_pos_;
      size_t this_copy = std::min(bytes_to_copy, buffer_head_remaining);

      memcpy(out_char, buffer_head.data() + buffer_head_pos_, this_copy);

      bytes_to_copy -= this_copy;
      out_char += this_copy;
      buffer_head_pos_ += this_copy;

      if (buffer_head_pos_ == buffer_head.size()) {
        bytes_buffered_ -= buffer_head.size();
        buffer_head_pos_ = 0;
        buffers_.pop();
        signal_reader = true;
      }
    }

    if (signal_reader)
      reader_.Signal();
  }

  bytes_read_ += return_size;

  return return_size;
}

const char* CurlDataSource::GetUri() const {
  const char* current_uri = "(unknown)";

  GetCurlInfo(true, CURLINFO_EFFECTIVE_URL, &current_uri, "effective URL");

  return current_uri;
}

void CurlDataSource::SetRequestProperty(const std::string& name,
                                        const std::string& value) {
  std::string header_value;  // "Name: value" format

  if (value.empty()) {
    // Special syntax to denote an empty header, see
    // https://curl.haxx.se/libcurl/c/CURLOPT_HTTPHEADER.html
    header_value.reserve(name.size() + 1);
    header_value.append(name);
    header_value.append(";");
  } else {
    header_value.reserve(name.size() + value.size() + 2 /* ": " */);
    header_value.append(name);
    header_value.append(": ");
    header_value.append(value);
  }

  auto insert_result =
      request_properties_.insert(std::make_pair(name, std::string()));

  // Either a header with this name already exists (overwrite), or it was just
  // added (need to fill in the value)
  insert_result.first->second = std::move(header_value);

  curl_request_headers_.reset();
}
void CurlDataSource::ClearRequestProperty(const std::string& name) {
  request_properties_.erase(name);
  curl_request_headers_.reset();
}
void CurlDataSource::ClearAllRequestProperties() {
  request_properties_.clear();
  curl_request_headers_.reset();
}

const std::multimap<std::string, std::string>*
CurlDataSource::GetResponseHeaders() const {
  if (!active_) {
    LOG(WARNING) << "Invalid call to GetResponseHeaders() without a request";
    return 0;
  }

  return &response_headers_;
}

int CurlDataSource::GetResponseCode() const {
  long http_code;

  if (!active_) {
    LOG(WARNING) << "Invalid call to GetResponseCode() without a request";
    return 0;
  }

  if (!GetCurlInfo(true, CURLINFO_RESPONSE_CODE, &http_code, "HTTP code")) {
    return 0;
  }

  return http_code;
}

HttpDataSourceError CurlDataSource::GetHttpError() const {
  return http_error_;
}

std::string CurlDataSource::ReadAllToString(size_t max_length) {
  std::string out;

  if (max_length == 0)
    max_length = max_buffer_size_;

  if (max_length > max_buffer_size_) {
    LOG(WARNING) << "Call to " << __FUNCTION__ << ": max_length too large";
    return out;
  }

  if (!active_) {
    LOG(WARNING) << "Invalid call to " << __FUNCTION__ << " without a request";
    return out;
  }

  {
    base::AutoLock buffer_autolock(buffer_lock_);

    DCHECK_EQ(bytes_read_, 0)
        << "Only call ReadAllToString() once, and do not mix with Read()";
    DCHECK_EQ(buffer_head_pos_, 0);

    want_full_buffer_ = true;

    BeforeLoaderWait();
    // Just let the whole thing buffer
    while (bytes_buffered_ < max_length && !eof_ && !load_error_) {
      writer_.Wait();
    }
    AfterLoaderWait();

    // Check for error cases. There is no good way to signal a problem, so we
    // return an empty string.
    if (load_error_) {
      LOG(WARNING) << __FUNCTION__ << " failed due to error";
      return out;
    } else if (!eof_) {
      LOG(ERROR) << __FUNCTION__ << " response too large";
      return out;
    }

    out.reserve(bytes_buffered_);

    while (!buffers_.empty()) {
      out.append(buffers_.front());
      buffers_.pop();
    }

    bytes_read_ += bytes_buffered_;
    buffer_head_pos_ = 0;
    bytes_buffered_ = 0;
  }

  reader_.Signal();

  VLOG(2) << __FUNCTION__ << " read total " << out.size();
  return out;
}

template <typename T>
bool CurlDataSource::SetCurlOption(bool continue_on_error,
                                   CURLoption option,
                                   T param,
                                   const char* option_description) {
  if (curl_easy_setopt(easy_.get(), option, param) != CURLE_OK) {
    if (continue_on_error) {
      LOG(INFO) << "Unable to set libcurl " << option_description
                << ". Continuing.";
    } else {
      LOG(WARNING) << "Unable to set libcurl " << option_description
                   << ". Failing.";
    }

    return false;
  }

  return true;
}

template <typename T>
bool CurlDataSource::GetCurlInfo(bool continue_on_error,
                                 CURLINFO info,
                                 T* param,
                                 const char* info_description) const {
  if (curl_easy_getinfo(easy_.get(), info, param) != CURLE_OK) {
    if (continue_on_error) {
      LOG(INFO) << "Unable to get libcurl " << info_description
                << ". Continuing.";
    } else {
      LOG(WARNING) << "Unable to get libcurl " << info_description
                   << ". Failing.";
    }

    return false;
  }

  return true;
}

ssize_t CurlDataSource::DataSourceError(HttpDataSourceError http_error) {
  // Simple for now, might need to be more complex later.
  http_error_ = http_error;
  return RESULT_IO_ERROR;
}

bool CurlDataSource::BuildRequestHeaders() {
  if (curl_request_headers_) {
    VLOG(8) << "Re-using request headers";
    // Already cached from previous request
    return true;
  }

  for (const auto& header : request_properties_) {
    struct curl_slist* append_result =
        curl_slist_append(curl_request_headers_.get(), header.second.c_str());
    if (!append_result) {
      LOG(WARNING) << "Can't add " << header.first << " header to CURL request";
      curl_request_headers_.reset();
      return false;
    }

    VLOG(8) << "Creating request header " << header.second;

    if (!curl_request_headers_) {
      // Only do this once, otherwise the list will keep getting freed.
      // It's probably possible to do this in a way that doesn't test
      // curl_request_headers_ each time through the loop, but
      // curl_slist_append() iterates through the whole list each time it's
      // called, which makes any optimization here irrelevant.
      curl_request_headers_.reset(append_result);
      VLOG(8) << "First request header";
    }
  }

  return true;
}

bool CurlDataSource::ProcessResponseHeader(base::StringPiece header_line) {
  // This assumes HTTP headers
  VLOG(9) << "Header line: " << header_line;

  if (curl_first_header_time_.is_zero()) {
    base::TimeTicks now = base::TimeTicks::Now();
    curl_first_header_time_ = now - curl_handoff_time_;
    curl_handoff_time_ = now;
  }

  DCHECK(!headers_done_.IsSignaled());

  base::AutoLock buffer_autolock(buffer_lock_);

  if (load_error_)
    return false;

  // Maybe a HTTP response code
  if (base::StartsWith(header_line, "HTTP/", base::CompareCase::SENSITIVE)) {
    DCHECK(end_of_headers_)
        << "Unexpected HTTP response '" << header_line << "', headers_seen_ "
        << headers_seen_ << ", end_of_headers_ " << end_of_headers_;

    end_of_headers_ = false;
    headers_seen_ = true;

    VLOG(8) << "Found HTTP response code: " << header_line;
    // We should clear the headers at this point in case this is a redirect,
    // otherwise we'll end up merging the headers from the two (or more)
    // responses
    response_headers_.clear();
    return true;
  }

  DCHECK(!end_of_headers_) << "Unexpected header '" << header_line
                           << "', headers_seen_ " << headers_seen_
                           << ", end_of_headers_ " << end_of_headers_;

  if (header_line.length() == 0) {
    VLOG(8) << "End of headers detected";
    end_of_headers_ = true;
    return true;
  }

  size_t separator = header_line.find(':');
  if (separator == base::StringPiece::npos) {
    VLOG(1) << "Invalid header, no ':' found (ignoring): " << header_line;
    return true;
  }

  base::StringPiece key = TrimWhitespaceASCII(header_line.substr(0, separator),
                                              base::TRIM_TRAILING);
  base::StringPiece value = TrimWhitespaceASCII(
      header_line.substr(separator + 1), base::TRIM_LEADING);

  VLOG(8) << "Header key: [" << key << "], value: [" << value << "]";

  response_headers_.insert(std::make_pair(key.as_string(), value.as_string()));
  return true;
}

bool CurlDataSource::ProcessHeadersComplete() {
  double len;

  if (GetCurlInfo(true, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len,
                  "download length")) {
    VLOG(3) << "Tentative length at body start: " << len;
    tentative_length_ = (len > 0 ? len : LENGTH_UNBOUNDED);
  } else {
    tentative_length_ = LENGTH_UNBOUNDED;
  }

  if (is_http_) {
    int response_code = GetResponseCode();
    if (response_code < 200 || response_code > 299) {
      LOG(INFO) << "HTTP error response code " << response_code;
      http_error_ = HTTP_RESPONSE_CODE_ERROR;
      return false;
    } else if (response_code == 200 && is_range_request_) {
      LOG(ERROR) << "Web server ignored byte range.";
      http_error_ = HTTP_CONTENT_TYPE_ERROR;
      return false;
    }
  }

  if (listener_) {
    listener_->OnTransferStart();
  }

  headers_done_.Signal();
  return true;
}

bool CurlDataSource::ProcessBodyData(const void* buffer, size_t size) {
  if (size == 0)
    return true;

  // Waiting until ProcessBodyData() to trigger the listener callback will
  // slightly under-estimate the rate when a request is cancelled or if there
  // is a problem processing the headers. On the other hand, fully accounting
  // for that raises the complexity in CurlWriteCallback().
  //
  // For a normal request, putting the trigger here will slightly over-estimate
  // the rate (not-so-slightly on small requests) because the presence of body
  // data is used to trigger the OnTransferStart() callback. As a result, the
  // time between transfer start and the first batch of data is approximately
  // zero, causing an over-estimation of the rate when the number of data
  // batches is low.
  //
  // We assume that in practice these factors balance out and/or become
  // negligable.
  if (listener_) {
    listener_->OnBytesTransferred(size);
  }

  base::AutoLock buffer_autolock(buffer_lock_);
  DCHECK(!is_http_ || (headers_seen_ && end_of_headers_))
      << "Body seen with wrong header state; "
      << "headers_seen_ " << headers_seen_ << ", end_of_headers_ "
      << end_of_headers_;

  const char* unread_buffer = reinterpret_cast<const char*>(buffer);
  size_t unread_size = size;
  bool buffer_was_empty = buffers_.empty();

  while (unread_size > 0) {
    if (load_error_)
      return false;

    if (GetBufferFree() > 0) {
      size_t write_size = std::min(unread_size, GetBufferFree());
      buffers_.emplace(unread_buffer, write_size);
      bytes_buffered_ += write_size;

      unread_buffer += write_size;
      unread_size -= write_size;

      DCHECK(GetBufferFree() == 0 || unread_size == 0);
    } else {
      while (GetBufferFree() <= 0 && !load_error_) {
        if (buffer_was_empty || want_full_buffer_) {
          writer_.Signal();
          buffer_was_empty = false;
        }

        BeforeCurlWait();
        reader_.Wait();
        AfterCurlWait();
      }
    }
  }

  if (buffer_was_empty && !buffers_.empty() && !want_full_buffer_)
    writer_.Signal();

  return true;
}

bool CurlDataSource::CheckCancel(const char* where) {
  if (cancel_ && cancel_->IsSet()) {
    VLOG(4) << "Cancel " << where;

    {
      base::AutoLock buffer_autolock(buffer_lock_);
      load_error_ = true;
    }
    reader_.Broadcast();
    writer_.Broadcast();

    return true;
  }

  return false;
}

void CurlDataSource::BeforeLoaderWait() {
  base::TimeTicks now = base::TimeTicks::Now();
  loader_processing_time_ += now - loader_handoff_time_;
  loader_handoff_time_ = now;
}

void CurlDataSource::AfterLoaderWait() {
  base::TimeTicks now = base::TimeTicks::Now();
  loader_waiting_time_ += now - loader_handoff_time_;
  loader_handoff_time_ = now;
}

void CurlDataSource::BeforeCurlWait() {
  base::TimeTicks now = base::TimeTicks::Now();
  curl_processing_time_ += now - curl_handoff_time_;
  curl_handoff_time_ = now;
}

void CurlDataSource::AfterCurlWait() {
  base::TimeTicks now = base::TimeTicks::Now();
  curl_waiting_time_ += now - curl_handoff_time_;
  curl_handoff_time_ = now;
}

void CurlDataSource::AfterCurlFinishWait() {
  base::TimeTicks now = base::TimeTicks::Now();
  curl_finish_time_ += now - curl_handoff_time_;
  curl_handoff_time_ = now;
}

}  // namespace upstream
}  // namespace ndash
