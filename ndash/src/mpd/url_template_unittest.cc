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

#include "url_template.h"

#include <map>
#include <string>

#include "gtest/gtest.h"

namespace ndash {
namespace mpd {

TEST(UrlTemplateTest, ConstructorArgs) {
  std::unique_ptr<UrlTemplate> templ;
  std::string expanded;

  templ = UrlTemplate::Compile("Leave me alone");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Leave me alone", expanded);

  templ = UrlTemplate::Compile("Edge1 $RepresentationID$");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge1 id", expanded);

  templ = UrlTemplate::Compile("Edge2 $Number$");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge2 1", expanded);

  templ = UrlTemplate::Compile("Edge3 $Bandwidth$");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge3 2", expanded);

  templ = UrlTemplate::Compile("Edge4 $Time$");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge4 3", expanded);

  templ = UrlTemplate::Compile(
      "Edge5 $RepresentationID$ $Number$ $Bandwidth$ $Time$");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge5 id 1 2 3", expanded);

  templ = UrlTemplate::Compile(
      "Edge5 $RepresentationID$ $Number$ $Bandwidth$ $Time$ Edge5");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge5 id 1 2 3 Edge5", expanded);

  templ = UrlTemplate::Compile("Edge6 $$ Edge6");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Edge6 $ Edge6", expanded);

  templ = UrlTemplate::Compile("Format $Number%04d$ Format");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Format 0001 Format", expanded);

  templ = UrlTemplate::Compile("Format $Number%0bad$ Format");
  expanded = templ->BuildUri("id", 1, 2, 3);
  EXPECT_EQ("Format 1 Format", expanded);
}

}  // namespace mpd
}  // namespace ndash
