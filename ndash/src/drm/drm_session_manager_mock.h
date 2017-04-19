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

#ifndef NDASH_DRM_DRM_SESSION_MANAGER_MOCK_H_
#define NDASH_DRM_DRM_SESSION_MANAGER_MOCK_H_

#include "drm/drm_session_manager.h"
#include "gmock/gmock.h"

namespace ndash {
namespace drm {

class MockDrmSessionManager : public DrmSessionManagerInterface {
 public:
  MockDrmSessionManager();
  ~MockDrmSessionManager() override;

  MOCK_METHOD1(SetDecoderCallbacks,
               void(const DashPlayerCallbacks* decoder_callbacks));
  MOCK_METHOD2(Request, void(const char* pssh_data, size_t pssh_len));
  MOCK_METHOD2(Join, bool(const char* pssh_data, size_t pssh_len));
};

}  // namespace drm
}  // namespace ndash

#endif  // NDASH_DRM_DRM_SESSION_MANAGER_MOCK_H_
