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

// Modification of statusor.h from the protobuf source tree.

// StatusOr<T> is the union of a Status object and a T
// object. StatusOr models the concept of an object that is either a
// usable value, or an error Status explaining why such a value is
// not present. To this end, StatusOr<T> does not allow its Status
// value to be Status::OK. Further, StatusOr<T*> does not allow the
// contained pointer to be nullptr.
//
// The primary use-case for StatusOr<T> is as the return value of a
// function which may fail.
//
// Example client usage for a StatusOr<T>, where T is not a pointer:
//
//  StatusOr<float> result = DoBigCalculationThatCouldFail();
//  if (result.ok()) {
//    float answer = result.ValueOrDie();
//    printf("Big calculation yielded: %f", answer);
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<T*>:
//
//  StatusOr<Foo*> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo = result.ConsumeValueOrDie();
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example factory implementation returning StatusOr<T*>:
//
//  StatusOr<Foo*> FooFactory::MakeNewFoo(int arg) {
//    if (arg <= 0) {
//      return util::Status(util::Code::INVALID_ARGUMENT,
//                            "Arg must be positive");
//    } else {
//      return new Foo(arg);
//    }
//  }
//

#ifndef UTIL_STATUSOR_H_
#define UTIL_STATUSOR_H_

#include <new>
#include <string>
#include <utility>

#include "util/status.h"

namespace util {

template <typename T,
          bool CopyConstructible = std::is_copy_constructible<T>::value>
class StatusOr {
  template <typename U, bool UC>
  friend class StatusOr;

 public:
  typedef T element_type;

  // Constructs a new StatusOr with Status::UNKNOWN status.  This is marked
  // 'explicit' to try to catch cases like 'return {};', where people think
  // util::StatusOr<std::vector<int>> will be initialized with an empty vector,
  // instead of a Status::UNKNOWN status.
  explicit StatusOr();

  // Construct a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to ValueOrDie() will fail.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do 'return
  // Status()' when the return type is StatusOr<T>.
  StatusOr(Status status);  // NOLINT

  // Construct a new StatusOr with the given value. If T is a plain pointer,
  // value must not be NULL. After calling this constructor, calls to
  // ValueOrDie() will succeed, and calls to status() will return OK.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when the return type is StatusOr<T>.
  StatusOr(const T& value);  // NOLINT

  // Copy constructor.
  StatusOr(const StatusOr& other) = default;

  // Conversion copy constructor, T must be copy constructible from U
  template <typename U>
  StatusOr(const StatusOr<U>& other);

  // Assignment operator.
  StatusOr& operator=(const StatusOr& other) = default;

  // Move constructor and move-assignment operator.
  StatusOr(StatusOr&& other) = default;
  StatusOr& operator=(StatusOr&& other) = default;

  // Rvalue-reference overloads of the other constructors and assignment
  // operators, to support move-only types and avoid unnecessary copying.
  //
  // Implementation note: we could avoid all these rvalue-reference overloads
  // if the existing lvalue-reference overloads took their arguments by value
  // instead. I think this would also let us omit the conversion assignment
  // operator altogether, since we'd get the same functionality for free
  // from the implicit conversion constructor and ordinary assignment.
  // However, this could result in extra copy operations unless we use
  // std::move to avoid them, and we can't use std::move because this code
  // needs to be portable to C++03.
  StatusOr(T&& value);  // NOLINT
  template <typename U>
  StatusOr(StatusOr<U>&& other);

  // Returns a reference to our status. If this contains a T, then
  // returns Status::OK.
  const Status& status() const { return status_; }

  // Returns this->status().ok()
  bool ok() const { return status_.ok(); }

  // Returns a reference to our current value, or fails if !this->ok().
  const T& ValueOrDie() const;
  T& ValueOrDie();

  // Moves our current value out of this object and returns it, or fails if
  // !this->ok().
  T ConsumeValueOrDie() { return std::move(ValueOrDie()); }

 private:
  Status status_;
  T value_;
};

// Partial specialization for when T is not copy-constructible. This uses all
// methods from the core implementation, but removes copy assignment and copy
// construction.
template <typename T>
class StatusOr<T, false> : public StatusOr<T, true> {
 public:
  // Remove copies.
  StatusOr(const StatusOr& other) = delete;
  StatusOr& operator=(const StatusOr& other) = delete;
  template <typename U>
  StatusOr(const StatusOr<U>& other) = delete;
  StatusOr(const T& value) = delete;

  // Use the superclass version for other constructors and operators.
  StatusOr() = default;
  StatusOr(StatusOr&& other) = default;
  StatusOr& operator=(StatusOr&& other) = default;
  StatusOr(T&& value)  // NOLINT
      : StatusOr<T, true>::StatusOr(std::move(value)) {}
  StatusOr(Status status)  // NOLINT
      : StatusOr<T, true>::StatusOr(std::move(status)) {}
  template <typename U>
  StatusOr(StatusOr<U>&& other)  // NOLINT
      : StatusOr<T, true>::StatusOr(std::move(other)) {}
};

////////////////////////////////////////////////////////////////////////////////
// Implementation details for StatusOr<T>

namespace internal {

class StatusOrHelper {
 public:
  // Move type-agnostic error handling to the .cc.
  static Status HandleInvalidStatusCtorArg();
  static Status HandleNullObjectCtorArg();
  static void Crash(const Status& status);

  // Customized behavior for StatusOr<T> vs. StatusOr<T*>
  template <typename T>
  struct Specialize;
};

template <typename T>
struct StatusOrHelper::Specialize {
  // For non-pointer T, a reference can never be NULL.
  static inline bool IsValueNull(const T& t) { return false; }
};

template <typename T>
struct StatusOrHelper::Specialize<T*> {
  static inline bool IsValueNull(const T* t) { return t == NULL; }
};

}  // namespace internal

template <typename T, bool CopyConstructible>
inline StatusOr<T, CopyConstructible>::StatusOr()
    : status_(Code::UNKNOWN, "") {}

template <typename T, bool CopyConstructible>
inline StatusOr<T, CopyConstructible>::StatusOr(Status status)
    : status_(std::move(status)) {
  if (status_.ok()) {
    status_ = internal::StatusOrHelper::HandleInvalidStatusCtorArg();
  }
}

template <typename T, bool CopyConstructible>
inline StatusOr<T, CopyConstructible>::StatusOr(const T& value)
    : value_(value) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(value)) {
    status_ = internal::StatusOrHelper::HandleNullObjectCtorArg();
  }
}

template <typename T, bool CopyConstructible>
template <typename U>
inline StatusOr<T, CopyConstructible>::StatusOr(const StatusOr<U>& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T, bool CopyConstructible>
inline StatusOr<T, CopyConstructible>::StatusOr(T&& value)
    : value_(std::move(value)) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(value_)) {
    status_ = internal::StatusOrHelper::HandleNullObjectCtorArg();
  }
}

template <typename T, bool CopyConstructible>
template <typename U>
inline StatusOr<T, CopyConstructible>::StatusOr(StatusOr<U>&& other)
    : status_(std::move(other.status_)), value_(std::move(other.value_)) {}

template <typename T, bool CopyConstructible>
inline const T& StatusOr<T, CopyConstructible>::ValueOrDie() const {
  if (!ok()) {
    internal::StatusOrHelper::Crash(status());
  }
  return value_;
}

template <typename T, bool CopyConstructible>
inline T& StatusOr<T, CopyConstructible>::ValueOrDie() {
  if (!status_.ok()) {
    internal::StatusOrHelper::Crash(status());
  }
  return value_;
}

}  // namespace util

#endif  // UTIL_STATUSOR_H_
