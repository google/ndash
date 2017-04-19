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

#ifndef NDASH_UPSTREAM_TRANSFER_LISTENER_H_
#define NDASH_UPSTREAM_TRANSFER_LISTENER_H_

#include <cstdint>

namespace ndash {
namespace upstream {

// Interface definition for a callback to be notified of data transfer events.
// Multiple transfers can be in progress at a time; the implementation will
// track the number of ongoing transfers via OnTransferStart/OnTransferEnd.
class TransferListenerInterface {
 public:
  virtual ~TransferListenerInterface() {}

  // Invoked when a transfer starts.
  virtual void OnTransferStart() = 0;

  // Called incrementally during a transfer.
  //
  // bytes: The number of bytes transferred since the previous call to this
  //         method (or if the first call, since the transfer was started).
  virtual void OnBytesTransferred(int32_t bytes) = 0;

  // Invoked when a transfer ends.
  virtual void OnTransferEnd() = 0;

 protected:
  TransferListenerInterface() {}

  TransferListenerInterface(const TransferListenerInterface& other) = delete;
  TransferListenerInterface& operator=(const TransferListenerInterface& other) =
      delete;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_TRANSFER_LISTENER_H_
