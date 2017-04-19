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

#include "manifest_fetcher.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread.h"
#include "gtest/gtest.h"
#include "manifest_fetcher.h"
#include "test/test_data.h"

namespace ndash {

TEST(ManifestFetcherTests, RequestRefreshTest) {
  base::WaitableEvent finish_waitable(true, false);

  class TestFetcherThread : public base::Thread, EventListenerInterface {
   public:
    TestFetcherThread(const std::string& name, base::WaitableEvent* waitable)
        : Thread(name),
          refresh_started_called_(false),
          manifest_was_refreshed_(false),
          waitable_(waitable) {}
    void BeginTest() {
      base::FilePath path(FLAGS_test_data_path);
      path = path.AppendASCII("mpd/data/ivod_sl_manifest.xml");
      std::string full_path = "file://" + path.AsUTF8Unsafe();
      fetcher_ = std::unique_ptr<ManifestFetcher>(
          new ManifestFetcher(full_path, task_runner(), this));
      ASSERT_TRUE(fetcher_->RequestRefresh());
    }
    void OnManifestRefreshStarted() override { refresh_started_called_ = true; }
    void OnManifestRefreshed() override {
      const mpd::MediaPresentationDescription* mpd;
      mpd = fetcher_->GetManifest();
      if (mpd != nullptr) {
        EXPECT_TRUE(mpd->GetPeriodCount() > 0);
        EXPECT_TRUE(refresh_started_called_);
        manifest_was_refreshed_ = true;
      } else {
        ADD_FAILURE() << "Could not parse manifest.";
      }
      waitable_->Signal();
    }
    void OnManifestError(ManifestFetcher::ManifestFetchError error) override {
      EXPECT_TRUE(false);
      waitable_->Signal();
    }
    bool GetManifestWasRefreshed() { return manifest_was_refreshed_; }

   private:
    std::unique_ptr<ManifestFetcher> fetcher_;
    bool refresh_started_called_;
    bool manifest_was_refreshed_;
    base::WaitableEvent* waitable_;
  };

  TestFetcherThread t("test_thread", &finish_waitable);
  t.Start();
  t.task_runner()->PostTask(FROM_HERE, base::Bind(&TestFetcherThread::BeginTest,
                                                  base::Unretained(&t)));
  finish_waitable.Wait();
  EXPECT_EQ(true, t.GetManifestWasRefreshed());
}

TEST(ManifestFetcherTests, RequestRefreshTooSoonAfterErrorTest) {
  class TestFetcherThread : public base::Thread, EventListenerInterface {
   public:
    TestFetcherThread(const std::string& name, base::WaitableEvent* waitable)
        : Thread(name),
          quick_refresh_succeeded_(false),
          num_manifest_errors_(0),
          waitable_(waitable) {}
    void BeginTest() {
      base::FilePath path(FLAGS_test_data_path);
      path = path.AppendASCII("mpd/data/does_not_exist.xml");
      std::string full_path = "file://" + path.AsUTF8Unsafe();
      fetcher_ = std::unique_ptr<ManifestFetcher>(
          new ManifestFetcher(full_path, task_runner(), this));
      ASSERT_TRUE(fetcher_->RequestRefresh());
      // Test continues in OnManifestError
    }
    void OnManifestRefreshStarted() override {}
    void OnManifestRefreshed() override { ASSERT_TRUE(false); }
    void OnManifestError(ManifestFetcher::ManifestFetchError error) override {
      if (num_manifest_errors_ == 0) {
        // Got the expected error. Try immediate RequestRefresh should return
        // true since we allow fast retry.
        ASSERT_TRUE(fetcher_->RequestRefresh());
        num_manifest_errors_ = 1;
      } else if (num_manifest_errors_ == 1) {
        // Got another expected error. Immediate RequestRefresh should now
        // false as it is too soon to retry.
        ASSERT_FALSE(fetcher_->RequestRefresh());
        num_manifest_errors_ = 2;
        waitable_->Signal();
      }
    }
    int GetNumManifestErrors() { return num_manifest_errors_; }

    bool GetQuickRefreshSucceeded() { return quick_refresh_succeeded_; }

   private:
    std::unique_ptr<ManifestFetcher> fetcher_;
    bool quick_refresh_succeeded_;
    int num_manifest_errors_;
    base::WaitableEvent* waitable_;
  };

  base::WaitableEvent finish_waitable(true, false);
  TestFetcherThread t("test_thread", &finish_waitable);
  t.Start();
  t.task_runner()->PostTask(FROM_HERE, base::Bind(&TestFetcherThread::BeginTest,
                                                  base::Unretained(&t)));
  finish_waitable.Wait();
  EXPECT_EQ(2, t.GetNumManifestErrors());
}

}  // namespace ndash
