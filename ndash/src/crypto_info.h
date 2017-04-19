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

#ifndef NDASH_CRYPTO_INFO_H_
#define NDASH_CRYPTO_INFO_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ndash {

class CryptoInfo {
 public:
  CryptoInfo();
  ~CryptoInfo();

  int32_t GetNumSubSamples() const { return num_sub_samples_; }
  void SetNumSubSamples(int32_t num_sub_samples) {
    num_sub_samples_ = num_sub_samples;
  }

  const std::vector<int32_t>& GetNumBytesClear() const {
    return num_bytes_of_clear_data_;
  }

  std::vector<int32_t>* MutableNumBytesClear() {
    return &num_bytes_of_clear_data_;
  }

  const std::vector<int32_t>& GetNumBytesEncrypted() const {
    return num_bytes_of_encrypted_data_;
  }
  std::vector<int32_t>* MutableNumBytesEncrypted() {
    return &num_bytes_of_encrypted_data_;
  }

  const std::string& GetKey() const { return key_; }
  std::string* MutableKey() { return &key_; }

  const std::vector<char>& GetIv() const { return iv_; }
  std::vector<char>* MutableIv() { return &iv_; }

 private:
  int32_t num_sub_samples_ = 0;
  std::vector<int32_t> num_bytes_of_clear_data_;
  std::vector<int32_t> num_bytes_of_encrypted_data_;
  std::string key_;
  std::vector<char> iv_;
};

}  // namespace ndash

#endif  // NDASH_CRYPTO_INFO_H_
