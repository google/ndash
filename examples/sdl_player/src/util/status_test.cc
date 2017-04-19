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

#include "util/status.h"

#include "gtest/gtest.h"

namespace util {
namespace {
TEST(Status, Empty) {
  Status status;
  EXPECT_EQ(Code::OK, Status::OK.error_code());
  EXPECT_EQ("OK", Status::OK.ToString());
}

TEST(Status, GenericCodes) {
  EXPECT_EQ(Code::OK, Status::OK.error_code());
  EXPECT_EQ(Code::CANCELLED, Status::CANCELLED.error_code());
  EXPECT_EQ(Code::UNKNOWN, Status::UNKNOWN.error_code());
}

TEST(Status, ConstructorZero) {
  Status status(Code::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, ErrorMessage) {
  Status status(Code::INVALID_ARGUMENT, "");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("", status.error_message());
  EXPECT_EQ("INVALID_ARGUMENT", status.ToString());
  status = Status(Code::INVALID_ARGUMENT, "msg");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("msg", status.error_message());
  EXPECT_EQ("INVALID_ARGUMENT:msg", status.ToString());
  status = Status(Code::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("", status.error_message());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, Copy) {
  Status a(Code::UNKNOWN, "message");
  Status b(a);
  ASSERT_EQ(a.ToString(), b.ToString());
}

TEST(Status, Assign) {
  Status a(Code::UNKNOWN, "message");
  Status b;
  b = a;
  ASSERT_EQ(a.ToString(), b.ToString());
}

TEST(Status, AssignEmpty) {
  Status a(Code::UNKNOWN, "message");
  Status b;
  a = b;
  ASSERT_EQ(std::string("OK"), a.ToString());
  ASSERT_TRUE(b.ok());
  ASSERT_TRUE(a.ok());
}

TEST(Status, EqualsOK) {
  ASSERT_EQ(Status::OK, Status());
}

TEST(Status, EqualsSame) {
  const Status a = Status(Code::CANCELLED, "message");
  const Status b = Status(Code::CANCELLED, "message");
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsCopy) {
  const Status a = Status(Code::CANCELLED, "message");
  const Status b = a;
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  const Status a = Status(Code::CANCELLED, "message");
  const Status b = Status(Code::UNKNOWN, "message");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsDifferentMessage) {
  const Status a = Status(Code::CANCELLED, "message");
  const Status b = Status(Code::CANCELLED, "another");
  ASSERT_NE(a, b);
}
}  // namespace
}  // namespace util
