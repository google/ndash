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

#include "util/uri_util.h"

#include <string>

#include "gtest/gtest.h"

namespace ndash {

namespace util {

TEST(UriUtilTest, ResolverTests) {
  std::string baseUri = "http://somewhere/";
  std::string resolved;

  resolved = UriUtil::Resolve(baseUri, "http://absolute/path");
  EXPECT_EQ(resolved, "http://absolute/path");

  resolved = UriUtil::Resolve(baseUri, "");
  EXPECT_EQ(resolved, "http://somewhere/");

  std::string baseUriWithFragment = "http://somewhere#disappear";
  resolved = UriUtil::Resolve(baseUriWithFragment, "#survived");
  EXPECT_EQ(resolved, "http://somewhere#survived");

  std::string baseUriWithQuery = "http://somewhere?disappear=this";
  resolved = UriUtil::Resolve(baseUriWithQuery, "?survived=that");
  EXPECT_EQ(resolved, "http://somewhere?survived=that");

  resolved = UriUtil::Resolve(baseUri, "//authority/here");
  EXPECT_EQ(resolved, "http://authority/here");

  resolved = UriUtil::Resolve(baseUri, "/some/path");
  EXPECT_EQ(resolved, "http://somewhere/some/path");

  resolved = UriUtil::Resolve(baseUri, "/some/./path");
  EXPECT_EQ(resolved, "http://somewhere/some/path");

  resolved = UriUtil::Resolve(baseUri, "/some/other/../path");
  EXPECT_EQ(resolved, "http://somewhere/some/path");

  std::string baseUriNoTrailing = "http://somewhere";
  resolved = UriUtil::Resolve(baseUriNoTrailing, "appendme");
  EXPECT_EQ(resolved, "http://somewhere/appendme");

  resolved = UriUtil::Resolve(baseUri, "appendme");
  EXPECT_EQ(resolved, "http://somewhere/appendme");
}

TEST(UriUtilTest, GetQueryParam) {
  std::string url = "https://manifest.host.com";
  EXPECT_EQ("", UriUtil::GetQueryParam(url, "param1"));

  url = "https://manifest.host.com?";
  EXPECT_EQ("", UriUtil::GetQueryParam(url, "param1"));

  url = "https://manifest.host.com?param1=a";
  EXPECT_EQ("a", UriUtil::GetQueryParam(url, "param1"));

  url = "https://manifest.host.com?param1=&param2=c";
  EXPECT_EQ("", UriUtil::GetQueryParam(url, "param1"));

  url = "https://manifest.host.com?param1=a&param2=bcd#fragment";
  EXPECT_EQ("a", UriUtil::GetQueryParam(url, "param1"));
  EXPECT_EQ("bcd", UriUtil::GetQueryParam(url, "param2"));

  url = "https://manifest.host.com?param1=a&param2=bcd";
  EXPECT_EQ("a", UriUtil::GetQueryParam(url, "param1"));
  EXPECT_EQ("bcd", UriUtil::GetQueryParam(url, "param2"));
}

TEST(UriUtilTest, RemoveQueryParam) {
  std::string url_out;

  std::string url = "https://manifest.host.com";
  EXPECT_EQ(url, UriUtil::RemoveQueryParam(url, "param1"));

  url = "https://manifest.host.com?";
  EXPECT_EQ(url, UriUtil::RemoveQueryParam(url, "param1"));

  url = "https://manifest.host.com?param1=a";
  url_out = "https://manifest.host.com?";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param1"));

  url = "https://manifest.host.com?param1=&param2=c";
  url_out = "https://manifest.host.com?param2=c";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param1"));

  url = "https://manifest.host.com?param1=a&param2=bcd#fragment";
  url_out = "https://manifest.host.com?param2=bcd#fragment";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param1"));
  url_out = "https://manifest.host.com?param1=a#fragment";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param2"));

  url = "https://manifest.host.com?param1=a&param2=bcd&param3=def";
  url_out = "https://manifest.host.com?param2=bcd&param3=def";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param1"));
  url_out = "https://manifest.host.com?param1=a&param3=def";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param2"));
  url_out = "https://manifest.host.com?param1=a&param2=bcd";
  EXPECT_EQ(url_out, UriUtil::RemoveQueryParam(url, "param3"));
}

TEST(UriUtilTest, DecodeQueryComponent) {
  std::string component = "one+two%3Dthree%26four%20five%2B";
  std::string expect = "one two=three&four five+";
  EXPECT_EQ(expect, UriUtil::DecodeQueryComponent(component));
}

}  // namespace util

}  // namespace ndash
