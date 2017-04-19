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

#ifndef NDASH_UPSTREAM_LOADER_H_
#define NDASH_UPSTREAM_LOADER_H_

#include "base/callback.h"

namespace ndash {
namespace upstream {

enum LoaderOutcome {
  LOAD_COMPLETE,
  LOAD_CANCELED,
  LOAD_ERROR,
};

// Interface definition of an object that can be loaded using a Loader.
//
// Some implementations may be reusable (can be passed to a Loader multiple
// times), others may be single-shot (can only be passed to a Loader once).
// Code using LoadableInterface pointers will need to be careful not to violate
// the rules of the particular implementation being used.
class LoadableInterface {
 public:
  LoadableInterface() {}
  virtual ~LoadableInterface() {}

  // Cancels the load. This may be called from any thread that is allowed to
  // call LoaderInterface::CancelLoading() or destroy the LoaderInterface for
  // any loader(s) that this has been passed to, plus anything else with a
  // pointer to this Loadable.
  //
  // LoadableInterface::Load() should (but is not required to) try to return
  // soon after CancelLoad() has been called.
  virtual void CancelLoad() = 0;

  // Whether the load has been canceled.
  //
  // Returns true if the load has been canceled, false otherwise. This may be
  // called from any thread.
  //
  // If IsLoadCanceled() returns true, it must not be reset (i.e. revert to
  // returning false) until after the LoadDoneCallback has been called,
  // otherwise canceled Load()s may be erroneously reported as LOAD_ERROR.
  virtual bool IsLoadCanceled() const = 0;

  // Performs the load, returning on completion or cancelation.
  //
  // Returns true if the load succeeded, false otherwise (including if
  // canceled before completion). This will be called once per
  // LoaderInterface::StartLoading() call, on an arbitrary thread, unless
  // canceled before Load() can be called (in which case it will never be
  // called).
  virtual bool Load() = 0;
};

// Manages the background loading of Loadable objects.
//
// LoaderInterface methods and the destructor must be called from only one
// thread, unless external locking is used. Warning: if using external locking
// to allow multiple threads to use a LoaderInterface, that will potentially
// require LoadableInterface::CancelLoad() to be more complicated.
class LoaderInterface {
 public:
  using LoadDoneCallback =
      base::Callback<void(LoadableInterface*, LoaderOutcome)>;

  LoaderInterface() {}
  virtual ~LoaderInterface() {}

  // Start loading a Loadable.
  // A Loader instance can only load one Loadable at a time, and so this method
  // must not be called when another load is in progress. When the callback is
  // called, the load is considered to no longer be in progress (so
  // StartLoading() can be called from the callback).
  //
  // looper: The looper of the thread on which the callback should be invoked.
  // loadable: The Loadable to load. It will be run on an arbitrary thread.
  //           LoaderInterface does not take ownership of the Loadable; it must
  //           not be destroyed until the callback is invoked.
  // callback: A callback to invoke when the load completes for any reason. The
  //           callback will be called from the thread that called StartLoading.
  //
  // Returns true if the Loadable has been scheduled, false otherwise (for
  // instance, if a Loadable is already running or the Loadable could not be
  // posted for execution)
  virtual bool StartLoading(LoadableInterface* loadable,
                            const LoadDoneCallback& callback) = 0;

  // Whether the Loader is currently loading a Loadable.
  virtual bool IsLoading() const = 0;

  // Cancels the current load (if any).
  virtual void CancelLoading() = 0;

  LoaderInterface(const LoaderInterface& other) = delete;
  LoaderInterface& operator=(const LoaderInterface& other) = delete;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_LOADER_H_
