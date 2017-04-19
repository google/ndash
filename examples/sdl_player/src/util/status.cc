// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "util/status.h"

#include <ostream>
#include <utility>

namespace util {

namespace {
inline std::string CodeEnumToString(Code code) {
  switch (code) {
    case Code::OK:
      return "OK";
    case Code::CANCELLED:
      return "CANCELLED";
    case Code::UNKNOWN:
      return "UNKNOWN";
    case Code::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case Code::DEADLINE_EXCEEDED:
      return "DEADLINE_EXCEEDED";
    case Code::NOT_FOUND:
      return "NOT_FOUND";
    case Code::ALREADY_EXISTS:
      return "ALREADY_EXISTS";
    case Code::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case Code::UNAUTHENTICATED:
      return "UNAUTHENTICATED";
    case Code::RESOURCE_EXHAUSTED:
      return "RESOURCE_EXHAUSTED";
    case Code::FAILED_PRECONDITION:
      return "FAILED_PRECONDITION";
    case Code::ABORTED:
      return "ABORTED";
    case Code::OUT_OF_RANGE:
      return "OUT_OF_RANGE";
    case Code::UNIMPLEMENTED:
      return "UNIMPLEMENTED";
    case Code::INTERNAL:
      return "INTERNAL";
    case Code::UNAVAILABLE:
      return "UNAVAILABLE";
    case Code::DATA_LOSS:
      return "DATA_LOSS";
  }

  // No default clause, clang will abort if a code is missing from
  // above switch.
  return "UNKNOWN";
}
}  // namespace

const Status Status::OK = Status();
const Status Status::CANCELLED = Status(Code::CANCELLED, "");
const Status Status::UNKNOWN = Status(Code::UNKNOWN, "");

Status::Status() : error_code_(Code::OK) {}

Status::Status(Code error_code, const std::string& error_message)
    : error_code_(error_code) {
  if (error_code != Code::OK) {
    error_message_ = error_message;
  }
}

Status::Status(const Status& other)
    : error_code_(other.error_code_), error_message_(other.error_message_) {}

Status& Status::operator=(const Status& other) {
  error_code_ = other.error_code_;
  error_message_ = other.error_message_;
  return *this;
}

bool Status::operator==(const Status& x) const {
  return error_code_ == x.error_code_ && error_message_ == x.error_message_;
}

std::string Status::ToString() const {
  if (error_code_ == Code::OK) {
    return CodeEnumToString(Code::OK);
  } else {
    if (error_message_.empty()) {
      return CodeEnumToString(error_code_);
    } else {
      return CodeEnumToString(error_code_) + ":" + error_message_;
    }
  }
}

std::ostream& operator<<(std::ostream& os, const Status& x) {
  os << x.ToString();
  return os;
}

}  // namespace util
