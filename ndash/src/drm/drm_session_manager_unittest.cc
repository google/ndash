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

#include <string>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "drm/drm_session_manager_mock.h"
#include "gtest/gtest.h"

char kFakeSession[] = "ksess12345";

// Used to force one thread to complete before another.
base::WaitableEvent waitable(true, false);

// Determines whether simulated fetch license succeeds or fails.
bool request_will_succeed;

// Determines whether return from fetch license is blocked until signaled
// (true) or whether it should signal to indicate it is complete (false).
bool block_fetch;

bool got_close_call;

namespace {

DashCdmStatus OpenCdmSession(void* context, char** session_id, size_t* len) {
  *session_id = strdup(&kFakeSession[0]);
  *len = strlen(kFakeSession);
  return DASH_CDM_SUCCESS;
}

DashCdmStatus CloseCdmSession(void* context,
                              const char* session_id,
                              size_t len) {
  got_close_call = true;
  return DASH_CDM_SUCCESS;
}

DashCdmStatus FetchLicense(void* context,
                           const char* session_id,
                           size_t session_id_len,
                           const char* pssh,
                           size_t pssh_len) {
  if (block_fetch) {
    waitable.Wait();
  } else {
    waitable.Signal();
  }

  return request_will_succeed ? DASH_CDM_SUCCESS : DASH_CDM_FAILURE;
}

void SignalWaitable() {
  waitable.Signal();
}
}  // namespace

void InitCallbacks(DashPlayerCallbacks* callbacks,
                   bool expect_success,
                   bool block) {
  got_close_call = false;
  waitable.Reset();
  callbacks->open_cdm_session_func = &OpenCdmSession;
  callbacks->close_cdm_session_func = &CloseCdmSession;
  callbacks->fetch_license_func = &FetchLicense;
  request_will_succeed = expect_success;
  block_fetch = block;
}

namespace ndash {
namespace drm {

void JoinCalledBeforeComplete(bool expect_success) {
  std::string pssh = "abcdefg";
  DashPlayerCallbacks callbacks;
  InitCallbacks(&callbacks, expect_success, false);

  DrmSessionManager drm_session_manager(nullptr /* context */, &callbacks);

  drm_session_manager.Request(pssh.c_str(), pssh.length());

  // Wait for fetch to complete before calling join.
  waitable.Wait();

  bool status = drm_session_manager.Join(pssh.c_str(), pssh.length());

  EXPECT_EQ(expect_success, status);
}

void JoinCalledAfterComplete(bool expect_success) {
  std::string pssh = "abcdefg";
  DashPlayerCallbacks callbacks;
  InitCallbacks(&callbacks, expect_success, true);

  DrmSessionManager drm_session_manager(nullptr /* context */, &callbacks);

  drm_session_manager.Request(pssh.c_str(), pssh.length());

  base::Thread t("test");
  t.Start();

  // Simulate fetch completing in the future.
  t.task_runner()->PostDelayedTask(FROM_HERE, base::Bind(&SignalWaitable),
                                   base::TimeDelta::FromMilliseconds(1000));

  // Call join before fetch completes.
  bool status = drm_session_manager.Join(pssh.c_str(), pssh.length());

  EXPECT_EQ(expect_success, status);
}

// Tests the mock class can be instantiated.
TEST(DrmSessionManagerTests, CanInstantiateMock) {
  MockDrmSessionManager drm_session_manager;
}

// Tests join before the request completes returns success.
TEST(DrmSessionManagerTests, JoinBeforeCompleteSuccess) {
  JoinCalledBeforeComplete(true);
}

// Tests join after the request completes returns success.
TEST(DrmSessionManagerTests, JoinAfterCompleteSuccess) {
  JoinCalledAfterComplete(true);
}

// Tests join before the request completes returns failure.
TEST(DrmSessionManagerTests, JoinBeforeCompleteFail) {
  JoinCalledBeforeComplete(false);
}

// Tests join after the request completes returns failure.
TEST(DrmSessionManagerTests, JoinAfterCompleteFail) {
  JoinCalledAfterComplete(false);
}

// Makes sure we cleanup the sessions we opened
TEST(DrmSessionManagerTests, CleanupOnDestroy) {
  MockDrmSessionManager drm_session_manager;
  JoinCalledBeforeComplete(true);
  EXPECT_EQ(true, got_close_call);
}

}  // namespace drm
}  // namespace ndash
