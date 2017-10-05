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

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "upstream/constants.h"
#include "upstream/curl_data_source.h"
#include "upstream/data_spec.h"

namespace ndash {

constexpr size_t kBufSize = 8192;

ManifestLoadable::ManifestLoadable(const std::string& manifest_uri)
    : manifest_uri_(manifest_uri) {
  load_buffer_ = std::unique_ptr<char[]>(new char[kBufSize + 1]);
}

ManifestLoadable::~ManifestLoadable() {}

void ManifestLoadable::CancelLoad() {
  // Nothing to do.
}

bool ManifestLoadable::IsLoadCanceled() const {
  return false;
}

bool ManifestLoadable::Load() {
  upstream::Uri uri(manifest_uri_);
  upstream::DataSpec manifest_spec(uri);
  upstream::CurlDataSource data_source("manifest");
  if (data_source.Open(manifest_spec) != upstream::RESULT_IO_ERROR) {
    ssize_t num_read;

    do {
      num_read = data_source.Read(load_buffer_.get(), kBufSize - 1);
      if (num_read > 0) {
        load_buffer_[num_read] = '\0';
        manifest_xml_.append(load_buffer_.get());
      }
    } while (num_read != upstream::RESULT_END_OF_INPUT &&
             num_read != upstream::RESULT_IO_ERROR);
    data_source.Close();
    return true;
  }
  data_source.Close();
  return false;
}

const std::string& ManifestLoadable::GetManifestUri() const {
  return manifest_uri_;
}

const std::string& ManifestLoadable::GetManifestXml() const {
  return manifest_xml_;
}

ManifestFetcher::ManifestFetcher(const std::string& manifest_uri,
                                 scoped_refptr<base::TaskRunner> task_runner,
                                 EventListenerInterface* event_listener)
    : loader_("manifest_loader"),
      manifest_uri_(manifest_uri),
      task_runner_(task_runner),
      event_listener_(event_listener),
      load_error_(NONE),
      load_error_count_(0) {}

ManifestFetcher::~ManifestFetcher() {}

void ManifestFetcher::UpdateManifestUri(const std::string& manifest_uri) {
  manifest_uri_ = manifest_uri;
}

const scoped_refptr<mpd::MediaPresentationDescription>
ManifestFetcher::GetManifest() const {
  return manifest_;
}

base::TimeTicks ManifestFetcher::GetManifestLoadStartTimestamp() const {
  return manifest_load_start_timestamp_;
}

base::TimeTicks ManifestFetcher::GetManifestLoadCompleteTimestamp() const {
  return manifest_load_complete_timestamp_;
}

bool ManifestFetcher::RequestRefresh() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (load_error_ != NONE &&
      now <
          load_error_timestamp_ + base::TimeDelta::FromMilliseconds(
                                      GetRetryDelayMillis(load_error_count_))) {
    // The previous load failed, and it's too soon to try again.
    return false;
  }

  if (!loader_.IsLoading()) {
    // TODO(rmrossi): Consider re-using the loadable rather than creating one
    // with each request.
    current_loadable_ =
        std::unique_ptr<ManifestLoadable>(new ManifestLoadable(manifest_uri_));
    current_load_start_timestamp_ = now;
    loader_.StartLoading(
        current_loadable_.get(),
        base::Bind(&ManifestFetcher::LoadComplete, base::Unretained(this)));
    NotifyManifestRefreshStarted();
  }

  return true;
}

// Called by the thread that called StartLoading()
void ManifestFetcher::LoadComplete(upstream::LoadableInterface* loadable,
                                   upstream::LoaderOutcome outcome) {
  if (current_loadable_.get() != loadable) {
    // Stale event. Ignore.
    return;
  }

  switch (outcome) {
    case upstream::LOAD_COMPLETE:
      ProcessLoadCompleted();
      break;
    case upstream::LOAD_ERROR:
      ProcessLoadError();
      break;
    case upstream::LOAD_CANCELED:
      // Nothing to do;
      break;
  }
  current_loadable_.reset();
}

void ManifestFetcher::Enable() {
  if (enabled_count_++ == 0) {
    load_error_ = NONE;
    load_error_count_ = 0;
  }
}

void ManifestFetcher::Disable() {
  DCHECK_GT(enabled_count_, 0);
  if (--enabled_count_ == 0) {
    loader_.CancelLoading();
  }
}

void ManifestFetcher::ProcessLoadCompleted() {
  const std::string& xml = current_loadable_->GetManifestXml();

  base::TimeTicks now = base::TimeTicks::Now();
  manifest_ = parser_.Parse(current_loadable_->GetManifestUri(),
                            base::StringPiece(xml));

  if (manifest_.get() != nullptr) {
    manifest_load_start_timestamp_ = current_load_start_timestamp_;
    manifest_load_complete_timestamp_ = now;
    load_error_ = NONE;
    load_error_count_ = 0;
    NotifyManifestRefreshed();
  } else {
    // Turn this into an error.
    load_error_count_++;
    load_error_timestamp_ = now;
    load_error_ = PARSING_ERROR;
    NotifyManifestError(load_error_);
  }
}

void ManifestFetcher::ProcessLoadError() {
  base::TimeTicks now = base::TimeTicks::Now();
  load_error_count_++;
  load_error_timestamp_ = now;
  load_error_ = UNKNOWN_ERROR;
  NotifyManifestError(load_error_);
}

// We allow fast retry after the first error but implement increasing back-off
// thereafter.
uint64_t ManifestFetcher::GetRetryDelayMillis(int error_count) const {
  // TODO(rmrossi): These values should be configurable.
  return std::min((error_count - 1) * 1000, 5000);
}

void ManifestFetcher::NotifyManifestRefreshStarted() {
  if (event_listener_ != nullptr) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&EventListenerInterface::OnManifestRefreshStarted,
                              base::Unretained(event_listener_)));
  }
}

void ManifestFetcher::NotifyManifestRefreshed() {
  if (event_listener_ != nullptr) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&EventListenerInterface::OnManifestRefreshed,
                              base::Unretained(event_listener_)));
  }
}

void ManifestFetcher::NotifyManifestError(ManifestFetchError error) {
  if (event_listener_ != nullptr) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&EventListenerInterface::OnManifestError,
                              base::Unretained(event_listener_), error));
  }
}

}  // namespace ndash
