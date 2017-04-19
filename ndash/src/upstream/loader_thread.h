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

#ifndef NDASH_UPSTREAM_LOADER_THREAD_H_
#define NDASH_UPSTREAM_LOADER_THREAD_H_

#include <string>

#include "base/callback.h"
#include "base/threading/thread.h"
#include "upstream/loader.h"

namespace ndash {
namespace upstream {

class LoaderThread : public LoaderInterface {
 public:
  // thread_name: A name for the Loader's thread.
  LoaderThread(const std::string& thread_name);
  ~LoaderThread() override;

  // Must only be called on the same thread that constructs and destructs the
  // LoaderThread.
  bool StartLoading(LoadableInterface* loadable,
                    const LoadDoneCallback& callback) override;

  bool IsLoading() const override;

  void CancelLoading() override;

 private:
  void RunLoad();   // Runs in loader thread
  void DoneLoad();  // Runs outside of loader thread

  bool loading_ = false;
  bool started_ = false;

  LoadableInterface* current_loadable_ = nullptr;
  LoadDoneCallback callback_;

  // This is set by DoLoad() (inside the loader thread) but only otherwise
  // accessed from DoneLoad(), which "happens after", so no locking is
  // required.
  LoaderOutcome loadable_outcome_ = LOAD_ERROR;

  base::Thread thread_;

  base::WeakPtrFactory<LoaderThread> weak_ptr_factory_;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_LOADER_THREAD_H_
