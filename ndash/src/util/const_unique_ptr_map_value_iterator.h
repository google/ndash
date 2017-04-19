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

#ifndef NDASH_UTIL_CONST_UNIQUE_PTR_MAP_VALUE_ITERATOR_H_
#define NDASH_UTIL_CONST_UNIQUE_PTR_MAP_VALUE_ITERATOR_H_

#include <iterator>
#include <utility>

namespace ndash {
namespace util {

// Const unique_ptr map iterator: use with iterators similar to
// std::map<X, std::unique_ptr<Y>>::const_iterator
//
// This effectively results in something that behaves like a
// std::list<const Y*>::const_iterator. It does not give access to the element
// key.
template <typename Iter>
class ConstUniquePtrMapValueIterator {
 public:
  typedef std::bidirectional_iterator_tag iterator_category;
  typedef const typename Iter::value_type::second_type::element_type value_type;
  typedef const value_type& reference;
  typedef const value_type* pointer;
  typedef ptrdiff_t difference_type;

  typedef ConstUniquePtrMapValueIterator<Iter> Self;

  explicit ConstUniquePtrMapValueIterator(Iter ii) : ii_(ii) {}

  reference operator*() const { return *ii_->second; }
  pointer operator->() const { return ii_->second.get(); }
  Self& operator++() {
    ++ii_;
    return *this;
  }
  Self operator++(int) { return Self(ii_++); }
  Self& operator--() {
    --ii_;
    return *this;
  }
  Self operator--(int) { return Self(ii_--); }
  bool operator==(const Self& other) { return ii_ == other.ii_; }
  bool operator!=(const Self& other) { return ii_ != other.ii_; }
  void swap(Self& other) { swap(ii_, other.ii_); }
  friend void swap(Self& a, Self& b) { a.swap(b); }

 private:
  Iter ii_;
};

template <typename Map>
class ConstUniquePtrMapValueRange {
 public:
  typedef ConstUniquePtrMapValueIterator<typename Map::const_iterator> Iterator;

  ConstUniquePtrMapValueRange(const Map* map) : map_(map) {}

  Iterator begin() { return Iterator(map_->cbegin()); }
  Iterator end() { return Iterator(map_->cend()); }

 private:
  const Map* map_;
};

}  // namespace util
}  // namespace ndash

#endif  // NDASH_UTIL_CONST_UNIQUE_PTR_VALUE_ITERATOR_H_
