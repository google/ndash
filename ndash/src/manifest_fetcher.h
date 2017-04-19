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

#ifndef NDASH_MANIFEST_FETCHER_H_
#define NDASH_MANIFEST_FETCHER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/task_runner.h"
#include "base/time/time.h"
#include "mpd/dash_manifest_representation_parser.h"
#include "mpd/media_presentation_description.h"
#include "upstream/loader_thread.h"

namespace ndash {

// A loadable that buffers a manifest xml document.
class ManifestLoadable : public upstream::LoadableInterface {
 public:
  ManifestLoadable(const std::string& manifest_uri);
  ~ManifestLoadable() override;

  void CancelLoad() override;
  bool IsLoadCanceled() const override;
  bool Load() override;
  const std::string& GetManifestUri() const;
  const std::string& GetManifestXml() const;

 private:
  std::unique_ptr<char[]> load_buffer_;
  std::string manifest_uri_;
  std::string manifest_xml_;
};

class EventListenerInterface;

// A utility class to fetch manifests and produce a
// MediaPresentationDescription object.
// Unless otherwise specified, methods are called by the main thread.
class ManifestFetcher {
 public:
  enum ManifestFetchError {
    NONE = 0,
    UNKNOWN_ERROR = -1,
    NETWORK_ERROR = -2,
    PARSING_ERROR = -3,
  };

  // Create a new manifest fetcher for the given manifest uri.
  // Callback events will be posted to the the given task_runner.
  ManifestFetcher(const std::string& manifest_uri,
                  scoped_refptr<base::TaskRunner> task_runner,
                  EventListenerInterface* event_listener = nullptr);
  ~ManifestFetcher();

  // Change the manifest uri this manifest fetcher is fetching from.
  void UpdateManifestUri(const std::string& manifest_uri);

  // Get a pointer to the most recent parsed manifest this manifest fetcher
  // is holding. May be null.
  const mpd::MediaPresentationDescription* GetManifest() const;

  // Returns true iff a manifest is held
  bool HasManifest() const { return manifest_ != nullptr; }

  // Return the timestamp when the most recent manifest fetch was started
  // according to the monotonic non-decreasing clock time.
  base::TimeTicks GetManifestLoadStartTimestamp() const;
  // Return the timestamp when the most recent manifest fetch was completed
  // according to the monotonic non-decreasing clock time.
  base::TimeTicks GetManifestLoadCompleteTimestamp() const;

  // Request this manifest fetcher refresh its manifest. If an error occurred
  // recently and not enough time has passed, returns false.  Otherwise, returns
  // true.
  bool RequestRefresh();

  void LoadComplete(upstream::LoadableInterface* loadable,
                    upstream::LoaderOutcome outcome);

  // Returns true if everything is OK, false otherwise
  bool CanContinueBuffering() const {
    return load_error_ == NONE || load_error_count_ <= 1;
  }

  // Enable/Disable counts usage, so the ManifestFetcher isn't shut down until
  // the number of calls to Disable() matches the number of calls to Enable()
  void Enable();
  void Disable();

 private:
  upstream::LoaderThread loader_;
  mpd::MediaPresentationDescriptionParser parser_;

  std::string manifest_uri_;
  scoped_refptr<base::TaskRunner> task_runner_;
  EventListenerInterface* event_listener_;
  ManifestFetchError load_error_;
  int load_error_count_;
  base::TimeTicks load_error_timestamp_;
  base::TimeTicks manifest_load_start_timestamp_;
  base::TimeTicks manifest_load_complete_timestamp_;
  std::unique_ptr<ManifestLoadable> current_loadable_;
  base::TimeTicks current_load_start_timestamp_;
  scoped_refptr<mpd::MediaPresentationDescription> manifest_;

  int enabled_count_ = 0;

  // Return how long we should delay between retries factoring in the current
  // error count.
  uint64_t GetRetryDelayMillis(int errorCount) const;

  // Helper functions for LoadComplete callback (on loader thread).
  void ProcessLoadCompleted();
  void ProcessLoadError();

  // Helper functions to call event listener methods.
  void NotifyManifestRefreshStarted();
  void NotifyManifestRefreshed();
  void NotifyManifestError(ManifestFetchError error);
};

// Interface definition for a callbacks to be notified of ManifestFetcher
// events.
// TODO(rmrossi): Consider using Base::Callback(s) instead of an interface.
class EventListenerInterface {
 public:
  EventListenerInterface(){};
  virtual ~EventListenerInterface(){};
  virtual void OnManifestRefreshStarted() = 0;
  virtual void OnManifestRefreshed() = 0;
  virtual void OnManifestError(ManifestFetcher::ManifestFetchError error) = 0;
};

}  // namespace ndash

#endif  // NDASH_MANIFEST_FETCHER_H_
