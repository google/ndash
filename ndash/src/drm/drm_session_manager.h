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

#ifndef NDASH_DRM_SESSION_MANAGER_H_
#define NDASH_DRM_SESSION_MANAGER_H_

#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ndash.h"

namespace ndash {

struct DashPlayerCallbackContextHolder;

namespace drm {

// Establishes CDM sessions and makes asynchronous requests for a
// playback licenses (through the CDM).  It keeps track of which PSSH boxes
// we already have licenses for.
class DrmSessionManagerInterface {
 public:
  DrmSessionManagerInterface() {}
  virtual ~DrmSessionManagerInterface() {}

  // Make an asynchronous license request for the given PSSH data. If a license
  // has already been fetched, returns immediately. Clients may wait for
  // in-flight requests to complete by calling the Join method below. Returns
  // false on fatal error.  Should be called by the sample producer
  // thread.
  virtual void Request(const char* pssh_data, size_t pssh_len) = 0;

  // If a license for the given key is already fetched, returns true
  // immediately. If any requests are pending responses, blocks until
  // they are complete (after retries).  Returns true if a license
  // is fetched, false otherwise. Should be called by the sample consumer
  // thread.
  virtual bool Join(const char* pssh_data, size_t pssh_len) = 0;
};

// An implentation of a DrmSessionManagerInterface
class DrmSessionManager : public DrmSessionManagerInterface {
 public:
  explicit DrmSessionManager(void** context_ptr,
                             const DashPlayerCallbacks* decoder_callbacks);
  ~DrmSessionManager() override;

  void Request(const char* pssh_data, size_t pssh_len) override;
  bool Join(const char* pssh_data, size_t pssh_len) override;

 private:
  struct CdmSessionContext {
    // A non-empty cdm_session_id_ indicates the CDM already has a license
    // for the associated PSSH.
    std::string cdm_session_id_;

    // A non-null value indicates a request for a license is in-flight or
    // has completed. A Join() call will block on this waitable until is has
    // been signalled by the completion of a request.
    std::unique_ptr<base::WaitableEvent> waitable_;
  };

  void *GetContext() const;

  void Run(CdmSessionContext* session_context, const std::string& pssh);

  void** context_ptr_;
  const DashPlayerCallbacks* decoder_callbacks_;

  // Maps PSSH blob to a session context structure describing the status
  // with respect to license availability for that PSSH.
  std::map<std::string, std::unique_ptr<CdmSessionContext>> pssh_sessions_;

  // TODO(rmrossi): Change this to a base::SequencedWorkerPool and use
  // PostWorkerTask() method so we can fetch licenses in parallel. (Also, make
  // sure that CDM can handle multiple threads calling into it. Might want to
  // have a cdm capabilities flag to ask it how many concurrent threads are
  // allowed so we can configure our pool properly).
  base::Thread worker_thread_;

  // Lock to synchronize access.
  base::Lock lock_;
};

}  // namespace drm
}  // namespace ndash

#endif  // NDASH_DRM_SESSION_MANAGER_H_
