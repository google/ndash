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
#ifndef NDASH_UPSTREAM_HTTP_DATA_SOURCE_H_
#define NDASH_UPSTREAM_HTTP_DATA_SOURCE_H_

#include <list>
#include <map>
#include <string>

#include "upstream/uri_data_source.h"

namespace ndash {
namespace upstream {

enum HttpDataSourceError {
  HTTP_OK,
  HTTP_IO_ERROR,
  HTTP_CONTENT_TYPE_ERROR,
  HTTP_RESPONSE_CODE_ERROR,
};

// An HTTP specific extension to UriDataSource.
class HttpDataSourceInterface : public UriDataSourceInterface {
 public:
  HttpDataSourceInterface() {}
  ~HttpDataSourceInterface() override {}

  // Sets the value of a request header field. The value will be used for
  // subsequent connections established by the source.
  virtual void SetRequestProperty(const std::string& name,
                                  const std::string& value) = 0;

  // Clears the value of a request header field. The change will apply to
  // subsequent connections established by the source.
  virtual void ClearRequestProperty(const std::string& name) = 0;

  // Clears all request header fields that were set by SetRequestProperty().
  virtual void ClearAllRequestProperties() = 0;

  // Gets the headers provided in the response.
  // Returns the response headers, or nullptr if response headers are
  // unavailable.
  virtual const std::multimap<std::string, std::string>* GetResponseHeaders()
      const = 0;

  // Gets the HTTP response code from the response
  virtual int GetResponseCode() const = 0;

  // Gets a more specific error code than Open() provides, in case of IO_ERROR
  // If no error, then returns HTTP_OK
  virtual HttpDataSourceError GetHttpError() const = 0;

  // Reads the entire remaining response into a string.
  //
  // max_length: The maximum length in bytes. If 0, then uses an
  //             implementation-defined maximum.
  //
  // If there is an error or if the response is too large, then an empty string
  // is returned.
  virtual std::string ReadAllToString(size_t max_length = 0) = 0;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_HTTP_DATA_SOURCE_H_
