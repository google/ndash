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

#include "drm/drm_session_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"

namespace ndash {
namespace drm {

DrmSessionManager::DrmSessionManager(
    void** context_ptr,
    const DashPlayerCallbacks* decoder_callbacks)
    : context_ptr_(context_ptr),
      decoder_callbacks_(decoder_callbacks),
      worker_thread_("CdmSessionThread") {
  worker_thread_.Start();
}

DrmSessionManager::~DrmSessionManager() {
  worker_thread_.Stop();
  for (auto iter = pssh_sessions_.begin(); iter != pssh_sessions_.end();
       ++iter) {
    CdmSessionContext* context = iter->second.get();
    std::string session_id = context->cdm_session_id_;
    // TODO(rmrossi): Why is CloseCdmSessionFunc not const char*?
    DashCdmStatus status = decoder_callbacks_->close_cdm_session_func(
        GetContext(), const_cast<char*>(session_id.c_str()),
        session_id.length());
    if (status != DASH_CDM_SUCCESS) {
      LOG(ERROR) << "Failed to close cdm session " << session_id;
    } else {
      LOG(INFO) << "Closed cdm session " << session_id;
    }
  }
}

void DrmSessionManager::Request(const char* pssh_data, size_t pssh_len) {
  base::AutoLock lock(lock_);
  std::string pssh(pssh_data, pssh_len);

  // Sanity checks.
  if (decoder_callbacks_ == nullptr) {
    LOG(ERROR) << "DrmSessionManager::SetDecoderCallbacks was not called";
    return;
  }

  if (decoder_callbacks_->open_cdm_session_func == nullptr) {
    LOG(ERROR)
        << "DrmSessionManager::open_cdm_session_func needs to be set via "
        << "DashThread::SetDecoderCallbacks";
    return;
  }

  if (decoder_callbacks_->fetch_license_func == nullptr) {
    LOG(ERROR) << "DrmSessionManager::fetch_license_func needs to be set via "
               << "DashThread::SetDecoderCallbacks";
    return;
  }

  VLOG(5) << "DrmSessionManager::check license requested";
  auto found = pssh_sessions_.find(pssh);
  CdmSessionContext* context = nullptr;
  if (found != pssh_sessions_.end()) {
    context = found->second.get();
    // Do we already have a license?
    if (!context->cdm_session_id_.empty()) {
      VLOG(5) << "DrmSessionManager::already have license";
      // Yes, nothing to do.
      return;
    }
    // A record exists but no session. There must be an in-flight
    // request already so there's nothing to do.
    DCHECK(context->waitable_.get() != nullptr);
    VLOG(5) << "DrmSessionManager::license request in-flight";
    return;
  } else {
    VLOG(5) << "DrmSessionManager::making asynch license request";
    context = new CdmSessionContext;
    context->waitable_ = std::unique_ptr<base::WaitableEvent>(
        new base::WaitableEvent(true, false));
    pssh_sessions_[pssh].reset(context);

    worker_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&DrmSessionManager::Run, base::Unretained(this),
                              context, std::move(pssh)));
  }
}

bool DrmSessionManager::Join(const char* pssh_data, size_t pssh_len) {
  std::string pssh(pssh_data, pssh_len);
  base::WaitableEvent* waitable = nullptr;

  VLOG(5) << "DrmSessionManager::license join called";
  {
    base::AutoLock lock(lock_);
    auto found = pssh_sessions_.find(pssh);
    if (found != pssh_sessions_.end()) {
      CdmSessionContext* context = found->second.get();
      if (!context->cdm_session_id_.empty()) {
        // We have a license.
        VLOG(5) << "DrmSessionManager::license arrived before join call";
        return true;
      }

      // A record exists but no session. There must be an in-flight
      // request. Wait for it to complete.
      waitable = context->waitable_.get();
    }
  }

  VLOG(5) << "DrmSessionManager::waiting for in-flight license to finish";
  DCHECK(waitable != nullptr);
  waitable->Wait();
  VLOG(5) << "DrmSessionManager::in-flight license request has finished";

  // Check whether we got a session.
  {
    base::AutoLock lock(lock_);
    auto found = pssh_sessions_.find(pssh);
    if (found != pssh_sessions_.end()) {
      CdmSessionContext* context = found->second.get();
      if (!context->cdm_session_id_.empty()) {
        return true;
      }
    }
  }

  return false;
}

void DrmSessionManager::Run(CdmSessionContext* session_context,
                            const std::string& pssh) {
  char* session_id;
  size_t session_id_len;
  DashCdmStatus status;

  // TODO(rmrossi): Add some retry logic here. Make number of retries or
  // total time elapsed before giving up configurable.  Make sure license
  // fetcher has a short timeout too (15s?)

  VLOG(5) << "DrmSessionManager::begin cdm license request";
  status = decoder_callbacks_->open_cdm_session_func(GetContext(), &session_id,
                                                     &session_id_len);
  // Take ownership of session_id storage and free it before we return.
  std::unique_ptr<char, decltype(free)*> owned_session_id(session_id, free);

  std::string session_id_str;

  if (status != DASH_CDM_SUCCESS) {
    LOG(ERROR) << "DrmSessionManager::failed top open cdm session";
    // Fall through to signal waitable with no session indicating failure.
  } else {
    session_id_str = std::string(session_id, session_id_len);
    status = decoder_callbacks_->fetch_license_func(
        GetContext(), session_id, session_id_len, pssh.c_str(), pssh.length());
    if (status != DASH_CDM_SUCCESS) {
      LOG(ERROR) << "DrmSessionManager::failed to fetch license";
      decoder_callbacks_->close_cdm_session_func(GetContext(), session_id,
                                                 session_id_len);
      session_id_str.clear();
      // Fall through to signal waitable with no session indicating failure.
    }
  }

  VLOG(5) << "DrmSessionManager::end cdm license request";
  {
    base::AutoLock lock(lock_);
    DCHECK(session_context != nullptr);
    session_context->cdm_session_id_ = session_id_str;
    session_context->waitable_->Signal();
  }
}
void* DrmSessionManager::GetContext() const {
  if (context_ptr_) {
    return *context_ptr_;
  } else {
    return nullptr;
  }
}

}  // namespace drm
}  // namespace ndash
