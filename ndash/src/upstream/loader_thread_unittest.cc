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

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upstream/loader_mock.h"
#include "upstream/loader_thread.h"

namespace ndash {
namespace upstream {

using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;

namespace {
class ClosureFunctor {
 public:
  explicit ClosureFunctor(const base::Closure& c) : c_(c) {}
  ClosureFunctor(const ClosureFunctor& f) : c_(f.c_) {}
  ~ClosureFunctor() {}
  ClosureFunctor& operator=(const ClosureFunctor& f) {
    c_ = f.c_;
    return *this;
  }
  void operator()() { c_.Run(); }

 private:
  base::Closure c_;
};

class MockCallbackReceiver {
 public:
  MockCallbackReceiver() {}
  virtual ~MockCallbackReceiver() {}

  MOCK_METHOD2(Callback, void(LoadableInterface*, LoaderOutcome));

  void QuitLoop() { quit_closure_.Run(); }

  base::Closure quit_closure_;
};

class Waiter {
 public:
  explicit Waiter(base::WaitableEvent* e) : e_(e) {}
  ~Waiter() {}

  void SignalEvent() { e_->Signal(); }

  void WaitForEvent() { e_->Wait(); }

 private:
  base::WaitableEvent* e_;
};

}  // namespace

TEST(LoaderThreadTest, TestBasicLoad) {
  base::MessageLoop top_loop;

  LoaderThread loader("TestBasicLoad loader");

  MockLoadableInterface loadable;
  MockCallbackReceiver callback_receiver;

  // Load success
  {
    base::RunLoop loop;

    {
      InSequence sequence;
      EXPECT_CALL(loadable, Load()).WillOnce(Return(true));
      EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_COMPLETE))
          .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));
    }

    EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(false));
    EXPECT_CALL(loadable, CancelLoad()).Times(0);

    EXPECT_THAT(loader.IsLoading(), Eq(false));

    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);

  // Load failure
  {
    base::RunLoop loop;

    {
      InSequence sequence;
      EXPECT_CALL(loadable, Load()).WillOnce(Return(false));
      EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_ERROR))
          .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));
    }

    EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(false));
    EXPECT_CALL(loadable, CancelLoad()).Times(0);

    EXPECT_THAT(loader.IsLoading(), Eq(false));

    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);
}

TEST(LoaderThreadTest, TestCancel) {
  base::MessageLoop top_loop;

  LoaderThread loader("TestCancel loader");

  MockLoadableInterface loadable;
  MockCallbackReceiver callback_receiver;

  // Cancel before run
  {
    base::RunLoop loop;

    EXPECT_CALL(loadable, Load()).Times(0);
    EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(true));
    EXPECT_CALL(loadable, CancelLoad()).Times(0);

    EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_CANCELED))
        .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));

    EXPECT_THAT(loader.IsLoading(), Eq(false));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);

  // Simulate Cancel through some mechanism external to LoaderInterface
  {
    base::RunLoop loop;

    {
      InSequence sequence;

      EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(false));
      EXPECT_CALL(loadable, Load()).Times(1);
      EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(true));
      EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_CANCELED))
          .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));
    }
    EXPECT_CALL(loadable, CancelLoad()).Times(0);

    EXPECT_THAT(loader.IsLoading(), Eq(false));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);
}

TEST(LoaderThreadTest, TestCallsDuringExecution) {
  base::MessageLoop top_loop;

  base::WaitableEvent e_load(true, false);
  Waiter waiter_load(&e_load);

  base::WaitableEvent e_finish(true, false);
  Waiter waiter_finish(&e_finish);

  LoaderThread loader("TestCallsDuringExecution loader");

  MockLoadableInterface loadable;
  MockCallbackReceiver callback_receiver;

  // Success in spite of cancel
  {
    base::RunLoop loop;

    Expectation load =
        EXPECT_CALL(loadable, Load())
            .WillOnce(
                DoAll(InvokeWithoutArgs(&waiter_load, &Waiter::SignalEvent),
                      InvokeWithoutArgs(&waiter_finish, &Waiter::WaitForEvent),
                      Return(true)));
    Expectation cancel = EXPECT_CALL(loadable, CancelLoad()).WillOnce(Return());

    EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_COMPLETE))
        .After(load, cancel)
        .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));

    EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(false));

    EXPECT_THAT(loader.IsLoading(), Eq(false));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    e_load.Wait();
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    loader.CancelLoading();
    e_finish.Signal();
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  e_load.Reset();
  e_finish.Reset();
  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);

  // Cancel results in cancel
  {
    base::RunLoop loop;

    {
      InSequence sequence;
      EXPECT_CALL(loadable, Load())
          .WillOnce(
              DoAll(InvokeWithoutArgs(&waiter_load, &Waiter::SignalEvent),
                    InvokeWithoutArgs(&waiter_finish, &Waiter::WaitForEvent),
                    Return(false)));
      EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_CANCELED))
          .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));
    }

    {
      InSequence sequence;
      EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(false));
      EXPECT_CALL(loadable, CancelLoad())
          .WillOnce(InvokeWithoutArgs(&waiter_finish, &Waiter::SignalEvent));
      EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(true));
    }

    EXPECT_THAT(loader.IsLoading(), Eq(false));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    e_load.Wait();
    EXPECT_THAT(loadable.IsLoadCanceled(), Eq(false));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    loader.CancelLoading();
    EXPECT_THAT(loadable.IsLoadCanceled(), Eq(true));
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  e_load.Reset();
  e_finish.Reset();
  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);

  // StartLoading when already started
  {
    base::RunLoop loop;

    {
      InSequence sequence;
      EXPECT_CALL(loadable, Load())
          .WillOnce(
              DoAll(InvokeWithoutArgs(&waiter_load, &Waiter::SignalEvent),
                    InvokeWithoutArgs(&waiter_finish, &Waiter::WaitForEvent),
                    Return(false)));
      EXPECT_CALL(callback_receiver, Callback(&loadable, LOAD_CANCELED))
          .WillOnce(InvokeWithoutArgs(ClosureFunctor(loop.QuitClosure())));
    }

    {
      InSequence sequence;
      EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(false));
      EXPECT_CALL(loadable, CancelLoad())
          .WillOnce(InvokeWithoutArgs(&waiter_finish, &Waiter::SignalEvent));
      EXPECT_CALL(loadable, IsLoadCanceled()).WillRepeatedly(Return(true));
    }

    EXPECT_THAT(loader.IsLoading(), Eq(false));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(true));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(false));
    e_load.Wait();
    EXPECT_THAT(loadable.IsLoadCanceled(), Eq(false));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(false));
    loader.CancelLoading();
    EXPECT_THAT(loadable.IsLoadCanceled(), Eq(true));
    EXPECT_THAT(loader.IsLoading(), Eq(true));
    EXPECT_THAT(
        loader.StartLoading(&loadable,
                            base::Bind(&MockCallbackReceiver::Callback,
                                       base::Unretained(&callback_receiver))),
        Eq(false));
    loop.Run();
    EXPECT_THAT(loader.IsLoading(), Eq(false));
  }

  e_load.Reset();
  e_finish.Reset();
  Mock::VerifyAndClearExpectations(&loadable);
  Mock::VerifyAndClearExpectations(&callback_receiver);
}

}  // namespace upstream
}  // namespace ndash
