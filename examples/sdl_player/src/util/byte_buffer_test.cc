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

#include "util/byte_buffer.h"

#include "gtest/gtest.h"

namespace util {

TEST(ByteBufferTest, EmptyBuffer) {
  PtsByteBuffer buffer;
  uint8_t read_buffer[4];
  int64_t pts = 123;
  EXPECT_EQ(buffer.Read(3, read_buffer, &pts), 0);
  EXPECT_EQ(pts, 123);
}

TEST(ByteBufferTest, PtsPushPop) {
  PtsByteBuffer buffer;
  std::string test_in_pts_1("ptsone");
  std::string test_in_pts_2("ptstwo");

  size_t one_size = test_in_pts_1.size();
  size_t two_size = test_in_pts_2.size();
  buffer.Write(one_size, (const uint8_t*)test_in_pts_1.c_str(), 1);
  buffer.Write(two_size, (const uint8_t*)test_in_pts_2.c_str(), 2);

  int64_t pts;

  EXPECT_EQ(buffer.Available(), 12);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // 3 byte-portion of first input.
  char read_buffer[64];
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(3, reinterpret_cast<uint8_t*>(read_buffer), &pts), 3);
  EXPECT_EQ(pts, 1);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "pts");
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // 4 bytes which overlap the last 3 bytes of the first and the first of the
  // second.
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(4, reinterpret_cast<uint8_t*>(read_buffer), &pts), 4);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "onep");
  EXPECT_EQ(pts, 1);

  // 2 bytes from the second.
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(2, reinterpret_cast<uint8_t*>(read_buffer), &pts), 2);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "ts");
  EXPECT_EQ(pts, 2);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // The last 3 bytes from the second, but request more than that to confirm
  // that we handle that correctly.
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(sizeof(read_buffer),
                        reinterpret_cast<uint8_t*>(read_buffer), &pts),
            3);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "two");
  EXPECT_EQ(pts, 2);
  EXPECT_EQ(buffer.Available(), 0);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());
}

TEST(ByteBufferTest, PtsPopMultipleBoundaries) {
  PtsByteBuffer buffer;
  std::string test_in_pts_1("ptsone");
  std::string test_in_pts_2("ptstwo");
  std::string test_in_pts_3("ptsthree");

  size_t one_size = test_in_pts_1.size();
  size_t two_size = test_in_pts_2.size();
  size_t three_size = test_in_pts_3.size();

  buffer.Write(one_size, (const uint8_t*)test_in_pts_1.c_str(), 1);
  buffer.Write(two_size, (const uint8_t*)test_in_pts_2.c_str(), 2);
  buffer.Write(three_size, (const uint8_t*)test_in_pts_3.c_str(), 3);

  EXPECT_EQ(buffer.Available(), 20);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  int64_t pts;
  char read_buffer[64];

  // Pull a couple bytes out first.
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(2, reinterpret_cast<uint8_t*>(read_buffer), &pts), 2);
  EXPECT_EQ(pts, 1);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "pt");
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // Then a single read that passes over multiple pts boundaries, but is started
  // by a pts of 1.
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(18, reinterpret_cast<uint8_t*>(read_buffer), &pts), 18);
  EXPECT_EQ(pts, 1);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "soneptstwoptsthree");
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());
}

TEST(ByteBufferTest, PtsPushPopPtsAligned) {
  PtsByteBuffer buffer;
  std::string test_in_pts_1("ptsone");
  std::string test_in_pts_2("ptstwo");

  size_t one_size = test_in_pts_1.size();
  size_t two_size = test_in_pts_2.size();
  buffer.Write(one_size, (const uint8_t*)test_in_pts_1.c_str(), 1);
  buffer.Write(two_size, (const uint8_t*)test_in_pts_2.c_str(), 2);

  EXPECT_EQ(buffer.Available(), 12);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  int64_t pts;
  char read_buffer[64];
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(
      buffer.Read(one_size, reinterpret_cast<uint8_t*>(read_buffer), &pts),
      one_size);
  EXPECT_EQ(pts, 1);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), test_in_pts_1.c_str());
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(
      buffer.Read(two_size, reinterpret_cast<uint8_t*>(read_buffer), &pts),
      two_size);
  EXPECT_EQ(pts, 2);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), test_in_pts_2.c_str());
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());
}

TEST(ByteBufferTest, PtsPushPopPtsMixedReadWrites) {
  PtsByteBuffer buffer;
  std::string test_in_pts_1("ptsone");
  std::string test_in_pts_2("ptstwo");
  std::string test_in_pts_3("ptsthree");

  size_t one_size = test_in_pts_1.size();
  size_t two_size = test_in_pts_2.size();
  size_t three_size = test_in_pts_3.size();

  // 3 byte-portion of first input.
  buffer.Write(one_size, (const uint8_t*)test_in_pts_1.c_str(), 1);
  int64_t pts;
  char read_buffer[64];
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(3, reinterpret_cast<uint8_t*>(read_buffer), &pts), 3);

  // Now write the next input, then read the rest of one, plus some.
  buffer.Write(two_size, (const uint8_t*)test_in_pts_2.c_str(), 2);
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(4, reinterpret_cast<uint8_t*>(read_buffer), &pts), 4);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "onep");
  EXPECT_EQ(pts, 1);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // 5 remaining bytes from the second.
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(5, reinterpret_cast<uint8_t*>(read_buffer), &pts), 5);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "tstwo");
  EXPECT_EQ(pts, 2);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // Should be empty now and read should return 0 bytes with pts and contents
  // the same.
  EXPECT_EQ(buffer.Available(), 0);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());
  EXPECT_EQ(buffer.Read(3, reinterpret_cast<uint8_t*>(read_buffer), &pts), 0);
  EXPECT_EQ(pts, 2);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "tstwo");

  // Write the last input and read 4 bytes of it.
  buffer.Write(three_size, (const uint8_t*)test_in_pts_3.c_str(), 3);
  bzero(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(buffer.Read(4, reinterpret_cast<uint8_t*>(read_buffer), &pts), 4);
  EXPECT_STREQ(reinterpret_cast<char*>(read_buffer), "ptst");
  EXPECT_EQ(pts, 3);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());

  // Some data should be remaining.
  EXPECT_EQ(buffer.Available(), 4);
  EXPECT_EQ(buffer.Available(), buffer.PtsDataAvailable());
}

}  // namespace util
