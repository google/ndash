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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "curl/curl.h"
#include "curl/easy.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upstream/data_spec.h"
#include "upstream/transfer_listener_mock.h"
#include "upstream/uri.h"

namespace ndash {
namespace upstream {

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pointwise;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

class CurlDataSourceTest : public ::testing::Test {
 protected:
  CurlDataSourceTest();
  ~CurlDataSourceTest() override;

  static CURL* GetEasy(CurlDataSource* cds) { return cds->easy_.get(); }
  static struct curl_slist* GetRequestHeaders(CurlDataSource* cds) {
    return cds->curl_request_headers_.get();
  }
  static void UnMakeHeadersAvailable(CurlDataSource* cds) {
    cds->active_ = false;
    cds->headers_done_.Reset();
  }
  static void MakeHeadersAvailable(CurlDataSource* cds) {
    cds->active_ = true;
    cds->headers_done_.Signal();
  }
  static std::map<std::string, std::string>* GetRequestProperties(
      CurlDataSource* cds) {
    return &cds->request_properties_;
  }
  static ssize_t SetHttpError(CurlDataSource* cds, enum HttpDataSourceError e) {
    return cds->DataSourceError(e);
  }
  static void BuildRequestHeaders(CurlDataSource* cds) {
    ASSERT_TRUE(cds->BuildRequestHeaders());
  }
  static void ProcessResponseHeader(CurlDataSource* cds,
                                    base::StringPiece header_line) {
    // Trim like CurlHeaderCallback
    cds->ProcessResponseHeader(
        base::TrimWhitespaceASCII(header_line, base::TRIM_ALL));
  }

  static void CurlSlistToList(const struct curl_slist* slist,
                              std::list<std::string>* list) {
    list->clear();

    const struct curl_slist* cur = slist;
    while (cur != nullptr) {
      list->emplace_back(cur->data);
      cur = cur->next;
    }
  }
  static void MapValuesToList(const std::map<std::string, std::string>& m,
                              std::list<std::string>* l) {
    l->clear();
    for (const auto& entry : m) {
      l->emplace_back(entry.second);
    }
  }
  static void RequestHeadersToList(CurlDataSource* cds,
                                   std::list<std::string>* list) {
    BuildRequestHeaders(cds);
    const struct curl_slist* slist = GetRequestHeaders(cds);
    CurlSlistToList(slist, list);
  }
  static void ProcessResponseHeaderList(CurlDataSource* cds,
                                        const std::list<std::string>& headers) {
    for (const std::string& line : headers) {
      ProcessResponseHeader(cds, base::StringPiece(line));
    }
  }

  const char* tempfile_name() const { return tempfile_name_; }

  FILE* CreateTempFile();
  void CleanupTempFile();

 private:
  char tempfile_name_[64] = {0};
  int tempfile_fd_ = -1;  // Automatically closed by fclose(tempfile_file_)
  FILE* tempfile_file_ = nullptr;
};

CurlDataSourceTest::CurlDataSourceTest() {}

CurlDataSourceTest::~CurlDataSourceTest() {
  CleanupTempFile();
}

FILE* CurlDataSourceTest::CreateTempFile() {
  EXPECT_TRUE(tempfile_file_ == nullptr);  // Clean up previous temp file first!
  if (tempfile_file_) {
    // Avoid leaving a mess in /tmp -- it's easy to clean up, even if it's
    // never supposed to happen
    CleanupTempFile();
  }

  strncpy(tempfile_name_, "/tmp/curl_data_source_test_XXXXXX.txt",
          sizeof(tempfile_name_) - 1);
  tempfile_name_[sizeof(tempfile_name_) - 1] = '\0';

  mode_t old_umask = umask(0177);  // Block group/world access

  tempfile_fd_ = mkostemps(tempfile_name_, 4, O_CLOEXEC);
  PCHECK(tempfile_fd_ >= 0) << "Failed creating temporary file";

  umask(old_umask);

  tempfile_file_ = fdopen(tempfile_fd_, "w+");
  PCHECK(tempfile_file_ != nullptr) << "Failed fdopen() of temporary file";

  return tempfile_file_;
}

void CurlDataSourceTest::CleanupTempFile() {
  if (strlen(tempfile_name_) > 0) {
    if (unlink(tempfile_name_)) {
      PLOG(WARNING) << "Failed deleting temporary file";
    }
    memset(tempfile_name_, 0, sizeof(tempfile_name_));
  }

  if (tempfile_file_) {
    fclose(tempfile_file_);
    tempfile_file_ = nullptr;
  }

  tempfile_fd_ = -1;  // Closed by fclose(), above
}

TEST_F(CurlDataSourceTest, CurlFromFileTest) {
  std::string file_uri_string("file://");
  static const char kFileContents[128] = "1234567890abcde\n";
  static const size_t kFileDataLength = 16;
  char file_read_buffer[128] = "";

  FILE* temp_file = CreateTempFile();
  EXPECT_EQ(kFileDataLength, strlen(kFileContents));
  file_uri_string.append(tempfile_name());
  fputs(kFileContents, temp_file);
  fflush(temp_file);

  Uri file_uri(file_uri_string);
  DataSpec file_spec(file_uri);

  CurlDataSource data_source("test");
  EXPECT_EQ(kFileDataLength, data_source.Open(file_spec));

  EXPECT_EQ(kFileDataLength,
            data_source.Read(file_read_buffer, sizeof(file_read_buffer)));
  static_assert(sizeof(file_read_buffer) == sizeof(kFileContents),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kFileContents));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(file_uri.uri(), std::string(current_uri));
  }

  data_source.Close();

  CleanupTempFile();
}

TEST_F(CurlDataSourceTest, CurlRangeFromFileTest) {
  std::string file_uri_string("file://");
  static const char kFileContents[128] = "1234567890abcde\n";
  static const size_t kFileDataLength = 16;
  static const char kRangeFileContents[128] = "34567";
  static const size_t kRangePosition = 2;
  static const size_t kRangeLength = 5;
  char file_read_buffer[128] = "";

  FILE* temp_file = CreateTempFile();
  EXPECT_EQ(kFileDataLength, strlen(kFileContents));
  file_uri_string.append(tempfile_name());
  fputs(kFileContents, temp_file);
  fflush(temp_file);

  Uri file_uri(file_uri_string);
  DataSpec file_spec(file_uri, kRangePosition, kRangeLength, NULL);

  CurlDataSource data_source("test");
  EXPECT_EQ(kRangeLength, data_source.Open(file_spec));

  EXPECT_EQ(kRangeLength,
            data_source.Read(file_read_buffer, sizeof(file_read_buffer)));
  static_assert(sizeof(file_read_buffer) == sizeof(kRangeFileContents),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kRangeFileContents));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(file_uri.uri(), std::string(current_uri));
  }

  data_source.Close();

  CleanupTempFile();
}

TEST_F(CurlDataSourceTest, ResponseHeaderTest) {
  CurlDataSource data_source("test");
  const std::multimap<std::string, std::string>* response_headers;
  std::multimap<std::string, std::string> expected_headers;

  MakeHeadersAvailable(&data_source);
  response_headers = data_source.GetResponseHeaders();
  ASSERT_THAT(response_headers, NotNull());
  EXPECT_THAT(*response_headers, IsEmpty());
  UnMakeHeadersAvailable(&data_source);

  const std::list<std::string> minimal_headers = {"HTTP/1.1 200 OK", ""};
  ProcessResponseHeaderList(&data_source, minimal_headers);
  MakeHeadersAvailable(&data_source);
  response_headers = data_source.GetResponseHeaders();
  ASSERT_THAT(response_headers, NotNull());
  EXPECT_THAT(*response_headers, IsEmpty());
  UnMakeHeadersAvailable(&data_source);

  const std::list<std::string> one_header = {"HTTP/1.1 200 OK", "Header: value",
                                             ""};
  expected_headers = {{"Header", "value"}};
  ProcessResponseHeaderList(&data_source, one_header);
  MakeHeadersAvailable(&data_source);
  response_headers = data_source.GetResponseHeaders();
  ASSERT_THAT(response_headers, NotNull());
  EXPECT_THAT(*response_headers, ContainerEq(expected_headers));
  UnMakeHeadersAvailable(&data_source);

  const std::list<std::string> many_headers = {
      "HTTP/1.1 200 OK",
      "Empty:",
      "EmptySpace: ",
      "Word: word",
      "WordNoSpace:word",
      "TrailingWhitespace:asdf    \t",
      "BothSidesWhitespace: \t both \t",
      "RepeatedHeader: first",
      "RepeatedHeader: second",
      "InternalWhitespace:    many words   here",
      "RepeatedHeader: third",
      ""};
  expected_headers = {
      // Note: this will be sorted because it's in a multimap
      {"Empty", ""},
      {"EmptySpace", ""},
      {"Word", "word"},
      {"WordNoSpace", "word"},
      {"TrailingWhitespace", "asdf"},
      {"BothSidesWhitespace", "both"},
      {"RepeatedHeader", "first"},
      {"RepeatedHeader", "second"},
      {"InternalWhitespace", "many words   here"},
      {"RepeatedHeader", "third"},
  };
  ProcessResponseHeaderList(&data_source, many_headers);
  MakeHeadersAvailable(&data_source);
  response_headers = data_source.GetResponseHeaders();
  ASSERT_THAT(response_headers, NotNull());
  EXPECT_THAT(*response_headers, ContainerEq(expected_headers));
  UnMakeHeadersAvailable(&data_source);
}

TEST_F(CurlDataSourceTest, RequestHeaderTest) {
  std::map<std::string, std::string> expected_properties;
  std::list<std::string> expected_headers;
  std::list<std::string> actual_headers;

  CurlDataSource data_source("test");
  const std::map<std::string, std::string>* request_properties =
      GetRequestProperties(&data_source);

  EXPECT_THAT(*request_properties, ContainerEq(expected_properties));
  BuildRequestHeaders(&data_source);
  EXPECT_THAT(GetRequestHeaders(&data_source), IsNull());

  data_source.SetRequestProperty("Deleted", "is gone");
  data_source.SetRequestProperty("Also-deleted", "gone too");

  expected_properties = {
      {"Also-deleted", "Also-deleted: gone too"},
      {"Deleted", "Deleted: is gone"},
  };
  EXPECT_THAT(*request_properties, ContainerEq(expected_properties));
  RequestHeadersToList(&data_source, &actual_headers);
  MapValuesToList(expected_properties, &expected_headers);
  EXPECT_THAT(actual_headers, ContainerEq(expected_headers));

  data_source.ClearAllRequestProperties();

  expected_properties.clear();
  EXPECT_THAT(*request_properties, ContainerEq(expected_properties));
  BuildRequestHeaders(&data_source);
  EXPECT_THAT(GetRequestHeaders(&data_source), IsNull());

  data_source.SetRequestProperty("Magic", "value");
  data_source.SetRequestProperty("Changed", "first");
  data_source.SetRequestProperty("Deleted", "again");

  expected_properties = {
      {"Changed", "Changed: first"},
      {"Deleted", "Deleted: again"},
      {"Magic", "Magic: value"},
  };
  EXPECT_THAT(*request_properties, ContainerEq(expected_properties));
  RequestHeadersToList(&data_source, &actual_headers);
  MapValuesToList(expected_properties, &expected_headers);
  EXPECT_THAT(actual_headers, ContainerEq(expected_headers));

  data_source.SetRequestProperty("Changed", "second");
  data_source.ClearRequestProperty("Deleted");
  data_source.SetRequestProperty("Empty", "");

  expected_properties = {
      {"Changed", "Changed: second"},
      {"Magic", "Magic: value"},
      {"Empty", "Empty;"},
  };
  EXPECT_THAT(*request_properties, ContainerEq(expected_properties));
  RequestHeadersToList(&data_source, &actual_headers);
  MapValuesToList(expected_properties, &expected_headers);
  EXPECT_THAT(actual_headers, ContainerEq(expected_headers));
}

TEST_F(CurlDataSourceTest, TransferListenerCallbacks) {
  StrictMock<MockTransferListener> listener;
  constexpr size_t kBufferLength = 128;

  std::string file_uri_string("file://");
  static const char kFileContents[kBufferLength] = "1234567890abcde\n";
  static const size_t kFileDataLength = 16;
  char file_read_buffer[kBufferLength] = "";

  FILE* temp_file = CreateTempFile();
  EXPECT_EQ(kFileDataLength, strlen(kFileContents));
  file_uri_string.append(tempfile_name());
  fputs(kFileContents, temp_file);
  fflush(temp_file);

  Uri file_uri(file_uri_string);
  DataSpec file_spec(file_uri);

  CurlDataSource data_source("test", &listener);

  // Normal read
  {
    InSequence seq;

    EXPECT_CALL(listener, OnTransferStart()).Times(1);
    EXPECT_CALL(listener, OnBytesTransferred(kFileDataLength)).Times(1);
    EXPECT_CALL(listener, OnTransferEnd()).Times(1);
  }

  EXPECT_EQ(kFileDataLength, data_source.Open(file_spec));

  EXPECT_EQ(kFileDataLength,
            data_source.Read(file_read_buffer, sizeof(file_read_buffer)));
  static_assert(sizeof(file_read_buffer) == sizeof(kFileContents),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kFileContents));

  data_source.Close();

  Mock::VerifyAndClearExpectations(&listener);

  // Range read
  static const char kRangeFileContents[kBufferLength] = "34567";
  static const size_t kRangePosition = 2;
  static const size_t kRangeLength = 5;

  memset(&file_read_buffer, 0, sizeof(file_read_buffer));

  {
    InSequence seq;

    EXPECT_CALL(listener, OnTransferStart()).Times(1);
    EXPECT_CALL(listener, OnBytesTransferred(kRangeLength)).Times(1);
    EXPECT_CALL(listener, OnTransferEnd()).Times(1);
  }

  DataSpec range_file_spec(file_uri, kRangePosition, kRangeLength, NULL);

  EXPECT_EQ(kRangeLength, data_source.Open(range_file_spec));

  EXPECT_EQ(kRangeLength,
            data_source.Read(file_read_buffer, sizeof(file_read_buffer)));
  static_assert(sizeof(file_read_buffer) == sizeof(kRangeFileContents),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kRangeFileContents));

  data_source.Close();

  Mock::VerifyAndClearExpectations(&listener);

  CleanupTempFile();
}

// Network tests are disabled because external network requests are not
// appropriate for unit tests. Enable these by hand to run them, or run with
// --gtest_also_run_disabled_tests
TEST(CurlDataSourceNetworkTest, DISABLED_HttpFileTest) {
  const std::string kHttpUriString("http://www.gstatic.com/robots.txt");
  const std::string kExpectedFirstLine = "User-agent: *\n";
  char file_read_buffer[128] = "";

  Uri http_uri(kHttpUriString);
  DataSpec http_spec(http_uri);

  CurlDataSource data_source("test");
  EXPECT_THAT(data_source.Open(http_spec), Ge(kExpectedFirstLine.size()));

  EXPECT_THAT(data_source.Read(file_read_buffer, sizeof(file_read_buffer)),
              Ge(kExpectedFirstLine.size()));
  EXPECT_THAT(file_read_buffer, StartsWith(kExpectedFirstLine));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(http_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(200, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_RangedHttpFileTest) {
  const std::string kHttpUriString("http://www.gstatic.com/robots.txt");
  static const char kExpectedString[128] = "agent";
  static const size_t kRangePosition = 5;
  static const size_t kRangeLength = 5;
  char file_read_buffer[128] = "";

  Uri http_uri(kHttpUriString);
  DataSpec http_spec(http_uri, kRangePosition, kRangeLength, NULL);

  CurlDataSource data_source("test");
  EXPECT_EQ(data_source.Open(http_spec), kRangeLength);

  EXPECT_EQ(data_source.Read(file_read_buffer, sizeof(file_read_buffer)),
            kRangeLength);
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kExpectedString));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(http_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(206, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_HttpEmptyTest) {
  const char kEmptyBuffer[128] = "";
  const std::string kHttpUriString("http://www.gstatic.com/generate_204");
  char file_read_buffer[128] = "";

  Uri http_uri(kHttpUriString);
  DataSpec http_spec(http_uri);

  CurlDataSource data_source("test");
  EXPECT_THAT(data_source.Open(http_spec), Eq(0));

  EXPECT_EQ(data_source.Read(file_read_buffer, sizeof(file_read_buffer)),
            RESULT_END_OF_INPUT);
  static_assert(sizeof(file_read_buffer) == sizeof(kEmptyBuffer),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kEmptyBuffer));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(http_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(204, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_HttpsEmptyTest) {
  const char kEmptyBuffer[128] = "";
  const std::string kHttpUriString("https://www.gstatic.com/generate_204");
  char file_read_buffer[128] = "";

  Uri http_uri(kHttpUriString);
  DataSpec http_spec(http_uri);

  CurlDataSource data_source("test");
  EXPECT_THAT(data_source.Open(http_spec), Eq(0));

  EXPECT_EQ(data_source.Read(file_read_buffer, sizeof(file_read_buffer)),
            RESULT_END_OF_INPUT);
  static_assert(sizeof(file_read_buffer) == sizeof(kEmptyBuffer),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kEmptyBuffer));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(http_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(204, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_HttpRedirectTo404Test) {
  const std::string kHttpUriString(
      "http://www.google.com/transparencyreport/safebrowsing/foo.html");
  const std::string kFinalUriString(
      "https://www.google.com/transparencyreport/safebrowsing/foo.html");

  Uri http_uri(kHttpUriString);
  Uri final_uri(kFinalUriString);
  DataSpec http_spec(http_uri);

  CurlDataSource data_source("test");
  EXPECT_THAT(data_source.Open(http_spec), Eq(RESULT_IO_ERROR));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(final_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(HTTP_RESPONSE_CODE_ERROR, data_source.GetHttpError());
  EXPECT_EQ(404, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_HttpRequestHeaderTest) {
  const std::string kHttpUriString("https://httpbin.org/headers");
  const std::string kExpectedOutput(
      "{\n"
      "  \"headers\": {\n"
      "    \"Accept\": \"*/*\", \n"
      "    \"Changed\": \"second\", \n"
      "    \"Empty\": \"\", \n"
      "    \"Host\": \"httpbin.org\", \n"
      "    \"Magic\": \"value\"\n"
      "  }\n"
      "}\n");

  char file_read_buffer[256] = "";

  Uri http_uri(kHttpUriString);
  DataSpec http_spec(http_uri);

  CurlDataSource data_source("test");
  data_source.SetRequestProperty("Deleted", "is gone");
  data_source.SetRequestProperty("Also-deleted", "gone too");
  data_source.ClearAllRequestProperties();
  data_source.SetRequestProperty("Magic", "value");
  data_source.SetRequestProperty("Changed", "first");
  data_source.SetRequestProperty("Deleted", "again");
  data_source.SetRequestProperty("Changed", "second");
  data_source.SetRequestProperty("Empty", "");
  data_source.ClearRequestProperty("Deleted");

  EXPECT_THAT(data_source.Open(http_spec), Eq(kExpectedOutput.size()));

  EXPECT_THAT(data_source.Read(file_read_buffer, sizeof(file_read_buffer)),
              Eq(kExpectedOutput.size()));
  std::string actual_output(
      file_read_buffer, strnlen(file_read_buffer, sizeof(file_read_buffer)));
  EXPECT_THAT(actual_output, StrEq(kExpectedOutput));

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(http_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(200, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_HttpPOSTTest) {
  const std::string kHttpUriString("http://httpbin.org/post");
  const std::string kPOSTBody = "arg1=one&arg2=two";
  const std::string kExpectedResponse =
      "\"arg1\": \"one\", \n    \"arg2\": \"two\"\n";
  char file_read_buffer[128] = "";

  Uri http_uri(kHttpUriString);
  DataSpec http_spec(http_uri, &kPOSTBody, 0, 0, LENGTH_UNBOUNDED, NULL, 0);

  CurlDataSource data_source("test");
  data_source.Open(http_spec);
  data_source.Read(file_read_buffer, sizeof(file_read_buffer));

  EXPECT_TRUE(strstr(file_read_buffer, kExpectedResponse.c_str()) != NULL);

  const char* current_uri = data_source.GetUri();
  EXPECT_TRUE(current_uri != nullptr);
  if (current_uri != nullptr) {
    EXPECT_EQ(http_uri.uri(), std::string(current_uri));
  }
  EXPECT_EQ(200, data_source.GetResponseCode());

  data_source.Close();
}

TEST(CurlDataSourceNetworkTest, DISABLED_TransferListenerCallback) {
  const std::string kRobotsHttpUriString("http://www.gstatic.com/robots.txt");
  const std::string kExpectedFirstLine = "User-agent: *\n";
  char file_read_buffer[128] = "";

  Uri robots_http_uri(kRobotsHttpUriString);
  DataSpec robots_http_spec(robots_http_uri);

  StrictMock<MockTransferListener> listener;
  int32_t bytes_transferred = 0;  // Accumulated from OnBytesTransferred()
  size_t total_read = 0;          // Accumulated from Read()
  CurlDataSource data_source("test", &listener);

  // HTTP GET
  {
    InSequence seq;

    EXPECT_CALL(listener, OnTransferStart()).Times(1);
    EXPECT_CALL(listener, OnBytesTransferred(Ge(0)))
        .WillRepeatedly(Invoke([&bytes_transferred](int32_t bytes) {
          bytes_transferred += bytes;
        }));
    EXPECT_CALL(listener, OnTransferEnd()).Times(1);
  }
  EXPECT_THAT(data_source.Open(robots_http_spec),
              Ge(kExpectedFirstLine.size()));

  size_t read_amount =
      data_source.Read(file_read_buffer, sizeof(file_read_buffer));
  EXPECT_THAT(read_amount, Ge(kExpectedFirstLine.size()));
  EXPECT_THAT(std::string(file_read_buffer, read_amount),
              StartsWith(kExpectedFirstLine));

  while (read_amount >= 0) {
    total_read += read_amount;
    read_amount = data_source.Read(file_read_buffer, sizeof(file_read_buffer));

    if (read_amount == RESULT_END_OF_INPUT) {
      break;
    }

    EXPECT_THAT(read_amount, Ge(0));
  }

  data_source.Close();

  Mock::VerifyAndClearExpectations(&listener);
  EXPECT_THAT(bytes_transferred, Eq(total_read));

  memset(file_read_buffer, 0, sizeof(file_read_buffer));
  bytes_transferred = 0;
  total_read = 0;

  static const char kExpectedString[128] = "agent";
  static const size_t kRangePosition = 5;
  static const size_t kRangeLength = 5;

  DataSpec robots_range_http_spec(robots_http_uri, kRangePosition, kRangeLength,
                                  NULL);

  {
    InSequence seq;

    EXPECT_CALL(listener, OnTransferStart()).Times(1);
    EXPECT_CALL(listener, OnBytesTransferred(Ge(0)))
        .WillRepeatedly(Invoke([&bytes_transferred](int32_t bytes) {
          bytes_transferred += bytes;
        }));
    EXPECT_CALL(listener, OnTransferEnd()).Times(1);
  }

  EXPECT_EQ(data_source.Open(robots_range_http_spec), kRangeLength);

  read_amount = data_source.Read(file_read_buffer, sizeof(file_read_buffer));
  EXPECT_THAT(read_amount, Eq(kRangeLength));
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kExpectedString));

  while (read_amount >= 0) {
    total_read += read_amount;
    read_amount = data_source.Read(file_read_buffer, sizeof(file_read_buffer));

    if (read_amount == RESULT_END_OF_INPUT) {
      break;
    }

    EXPECT_THAT(read_amount, Ge(0));
  }

  data_source.Close();

  Mock::VerifyAndClearExpectations(&listener);
  EXPECT_THAT(bytes_transferred, Eq(total_read));

  memset(file_read_buffer, 0, sizeof(file_read_buffer));
  bytes_transferred = 0;
  total_read = 0;

  const char kEmptyBuffer[128] = "";
  const std::string kEmptyHttpUriString("http://www.gstatic.com/generate_204");

  Uri empty_http_uri(kEmptyHttpUriString);
  DataSpec empty_http_spec(empty_http_uri);

  // Expect no TransferListener callbacks

  EXPECT_THAT(data_source.Open(empty_http_spec), Eq(0));

  EXPECT_EQ(data_source.Read(file_read_buffer, sizeof(file_read_buffer)),
            RESULT_END_OF_INPUT);
  static_assert(sizeof(file_read_buffer) == sizeof(kEmptyBuffer),
                "buffer sizes must match");
  EXPECT_THAT(file_read_buffer, Pointwise(Eq(), kEmptyBuffer));

  data_source.Close();

  const std::string kRedir404HttpUriString(
      "http://www.google.com/transparencyreport/safebrowsing/foo.html");

  Uri redir_404_http_uri(kRedir404HttpUriString);
  DataSpec redir_404_http_spec(redir_404_http_uri);

  EXPECT_THAT(data_source.Open(redir_404_http_spec), Eq(RESULT_IO_ERROR));
  data_source.Close();

  Mock::VerifyAndClearExpectations(&listener);
  EXPECT_THAT(bytes_transferred, Eq(total_read));

  memset(file_read_buffer, 0, sizeof(file_read_buffer));
  bytes_transferred = 0;
  total_read = 0;
}

}  // namespace upstream
}  // namespace ndash
