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

#include "upstream/loader_thread.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"

namespace ndash {
namespace upstream {

LoaderThread::LoaderThread(const std::string& thread_name)
    : callback_(), thread_(thread_name), weak_ptr_factory_(this) {}

LoaderThread::~LoaderThread() {
  if (started_) {
    thread_.Stop();
  }
}

bool LoaderThread::StartLoading(LoadableInterface* loadable,
                                const LoadDoneCallback& callback) {
  if (!started_) {
    started_ = thread_.Start();

    if (!started_) {
      LOG(WARNING) << "Couldn't start loader thread " << thread_.thread_name();
      return false;
    }
  }

  if (loading_) {
    return false;
  }

  loading_ = true;

  current_loadable_ = loadable;
  callback_ = callback;

  if (!thread_.task_runner()->PostTaskAndReply(
          FROM_HERE, base::Bind(&LoaderThread::RunLoad, base::Unretained(this)),
          base::Bind(&LoaderThread::DoneLoad,
                     weak_ptr_factory_.GetWeakPtr()))) {
    // Couldn't post the task, so run the callback immediately to report error
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LoaderThread::DoneLoad, weak_ptr_factory_.GetWeakPtr()));
  }

  return true;
}

bool LoaderThread::IsLoading() const {
  return loading_;
}

void LoaderThread::CancelLoading() {
  if (loading_) {
    current_loadable_->CancelLoad();
  }
}

void LoaderThread::RunLoad() {
  if (current_loadable_->IsLoadCanceled()) {
    loadable_outcome_ = LOAD_CANCELED;
    return;
  }

  bool result = current_loadable_->Load();

  if (current_loadable_->IsLoadCanceled()) {
    loadable_outcome_ = LOAD_CANCELED;
  } else if (result) {
    loadable_outcome_ = LOAD_COMPLETE;
  } else {
    loadable_outcome_ = LOAD_ERROR;
  }
}

void LoaderThread::DoneLoad() {
  // Save the callback parameters so that we can reset the class members. A
  // separate closure is created to allow callback_ to be reset. This needs to
  // be done in case the callback calls StartLoading().

  // Since DoneLoad is scheduled to run on the caller's thread, it's possible
  // the caller was able to invoke CancelLoading in between the time we finished
  // loading and when DoneLoad is ultimately called. Check again for the load
  // being canceled so the outcome is as the caller expects.
  if (current_loadable_->IsLoadCanceled()) {
    loadable_outcome_ = LOAD_CANCELED;
  }
  base::Closure done_closure =
      base::Bind(callback_, current_loadable_, loadable_outcome_);

  loading_ = false;
  current_loadable_ = nullptr;
  callback_.Reset();
  loadable_outcome_ = LOAD_ERROR;

  done_closure.Run();
}

}  // namespace upstream
}  // namespace ndash
