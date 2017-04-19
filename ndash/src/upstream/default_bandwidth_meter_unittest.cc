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

#include "upstream/default_bandwidth_meter.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upstream/bandwidth_meter.h"
#include "util/averager_mock.h"

namespace ndash {
namespace upstream {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

class MockSampleReceiver {
 public:
  MockSampleReceiver() {}
  virtual ~MockSampleReceiver() {}

  MOCK_METHOD3(BandwidthSampleCallback,
               void(base::TimeDelta elapsed, int64_t bytes, int64_t bitrate));
};

class DefaultBandwidthMeterTest : public ::testing::Test {
 protected:
  DefaultBandwidthMeterTest() {}
  ~DefaultBandwidthMeterTest() override {}

  BandwidthMeterInterface::BandwidthSampleCB BindSampleCallback() {
    if (sample_receiver_) {
      return base::Bind(&MockSampleReceiver::BandwidthSampleCallback,
                        base::Unretained(sample_receiver_.get()));
    } else {
      return BandwidthMeterInterface::BandwidthSampleCB();
    }
  }

  void ConstructTestBandwidthMeter(
      bool use_sample_receiver = true,
      int32_t max_weight = DefaultBandwidthMeter::kDefaultMaxWeight) {
    if (use_sample_receiver) {
      sample_receiver_.reset(new StrictMock<MockSampleReceiver>());
    }

    averager_ = new StrictMock<util::MockAverager>;
    owned_averager_.reset(averager_);

    test_clock_ = new base::SimpleTestTickClock;
    owned_tick_clock_.reset(test_clock_);

    meter_.reset(new DefaultBandwidthMeter(
        BindSampleCallback(), top_loop_.task_runner(),
        std::move(owned_tick_clock_), std::move(owned_averager_)));

    Mock::VerifyAndClearExpectations(sample_receiver_.get());
  }

  void VerifyAndClearMocks() {
    Mock::VerifyAndClearExpectations(sample_receiver_.get());
    Mock::VerifyAndClearExpectations(averager_);
  }

  base::MessageLoop top_loop_;

  util::MockAverager* averager_ = nullptr;
  base::SimpleTestTickClock* test_clock_ = nullptr;

  std::unique_ptr<MockSampleReceiver> sample_receiver_;
  std::unique_ptr<util::AveragerInterface> owned_averager_;
  std::unique_ptr<base::TickClock> owned_tick_clock_;
  std::unique_ptr<DefaultBandwidthMeter> meter_;
};

TEST_F(DefaultBandwidthMeterTest, NoSamples) {
  const base::TimeDelta kElapsed = base::TimeDelta::FromSeconds(1);
  ConstructTestBandwidthMeter();

  EXPECT_CALL(*averager_, AddSample(_, _)).Times(0);
  EXPECT_CALL(*averager_, GetAverage()).Times(0);
  EXPECT_CALL(*sample_receiver_, BandwidthSampleCallback(_, _, _)).Times(0);

  EXPECT_THAT(meter_->GetBitrateEstimate(),
              Eq(BandwidthMeterInterface::kNoEstimate));

  // 0 time passed, 0 bytes
  meter_->OnTransferStart();
  test_clock_->Advance(kElapsed);
  meter_->OnTransferEnd();

  EXPECT_THAT(meter_->GetBitrateEstimate(),
              Eq(BandwidthMeterInterface::kNoEstimate));

  // >0 time passed, 0 bytes
  meter_->OnTransferStart();
  test_clock_->Advance(kElapsed);
  meter_->OnTransferEnd();

  EXPECT_THAT(meter_->GetBitrateEstimate(),
              Eq(BandwidthMeterInterface::kNoEstimate));
}

TEST_F(DefaultBandwidthMeterTest, SingleStreams) {
  ConstructTestBandwidthMeter();

  const base::TimeDelta kElapsed1 = base::TimeDelta::FromSeconds(25);
  constexpr int64_t kBytes1 = 2500;
  constexpr int64_t kWeight1 = 50;
  constexpr int64_t kBitrate1 = 800;
  constexpr int64_t kResult1 = 123456;

  meter_->OnTransferStart();
  test_clock_->Advance(kElapsed1);

  // All in 1 transfer status
  meter_->OnBytesTransferred(kBytes1);

  {
    InSequence sequence;
    EXPECT_CALL(*averager_, AddSample(kWeight1, kBitrate1)).Times(1);
    EXPECT_CALL(*averager_, GetAverage()).WillRepeatedly(Return(kResult1));
  }
  EXPECT_CALL(*sample_receiver_,
              BandwidthSampleCallback(kElapsed1, kBytes1, kResult1))
      .Times(1);
  meter_->OnTransferEnd();

  {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  VerifyAndClearMocks();

  const base::TimeDelta kElapsed2 = base::TimeDelta::FromSeconds(36);
  constexpr int64_t kBytes2a = 600;
  constexpr int64_t kBytes2b = 2000;
  constexpr int64_t kBytes2c = 1000;
  constexpr int64_t kBytes2Total = 3600;
  constexpr int64_t kWeight2 = 60;
  constexpr int64_t kBitrate2 = 800;
  constexpr int64_t kResult2 = 543210;

  meter_->OnTransferStart();

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  meter_->OnBytesTransferred(kBytes2a);
  meter_->OnBytesTransferred(kBytes2b);
  meter_->OnBytesTransferred(kBytes2c);

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  test_clock_->Advance(kElapsed2);

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  {
    InSequence sequence;
    EXPECT_CALL(*averager_, AddSample(kWeight2, kBitrate2)).Times(1);
    EXPECT_CALL(*averager_, GetAverage()).WillRepeatedly(Return(kResult2));
  }
  EXPECT_CALL(*sample_receiver_,
              BandwidthSampleCallback(kElapsed2, kBytes2Total, kResult2))
      .Times(1);
  meter_->OnTransferEnd();

  {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult2));

  VerifyAndClearMocks();

  meter_->OnTransferStart();
  meter_->OnBytesTransferred(kBytes1);
  // Clock has not moved, so the transfer should be ignored
  EXPECT_CALL(*averager_, AddSample(_, _)).Times(0);
  EXPECT_CALL(*averager_, GetAverage()).Times(0);
  EXPECT_CALL(*sample_receiver_, BandwidthSampleCallback(_, _, _)).Times(0);
  meter_->OnTransferEnd();
}

TEST_F(DefaultBandwidthMeterTest, NegativeOrZeroEstimate) {
  ConstructTestBandwidthMeter(false);

  const base::TimeDelta kElapsed = base::TimeDelta::FromSeconds(1);
  constexpr int64_t kBytes = 123;
  constexpr int64_t kResult1 = -100;
  constexpr int64_t kResult2 = 0;

  meter_->OnTransferStart();
  test_clock_->Advance(kElapsed);
  meter_->OnBytesTransferred(kBytes);

  EXPECT_CALL(*averager_, AddSample(_, _)).Times(1);
  EXPECT_CALL(*averager_, GetAverage()).WillRepeatedly(Return(kResult1));

  meter_->OnTransferEnd();

  EXPECT_THAT(meter_->GetBitrateEstimate(),
              Eq(BandwidthMeterInterface::kNoEstimate));

  VerifyAndClearMocks();

  meter_->OnTransferStart();
  test_clock_->Advance(kElapsed);
  meter_->OnBytesTransferred(kBytes);

  EXPECT_CALL(*averager_, AddSample(_, _)).Times(1);
  EXPECT_CALL(*averager_, GetAverage()).WillRepeatedly(Return(kResult2));

  meter_->OnTransferEnd();

  EXPECT_THAT(meter_->GetBitrateEstimate(),
              Eq(BandwidthMeterInterface::kNoEstimate));
}

TEST_F(DefaultBandwidthMeterTest, ManyStreams) {
  ConstructTestBandwidthMeter();

  const base::TimeDelta kElapsed1 = base::TimeDelta::FromSeconds(25);
  constexpr int64_t kBytes1 = 2500;
  constexpr int64_t kWeight1 = 50;
  constexpr int64_t kBitrate1 = 800;
  constexpr int64_t kResult1 = 123456;

  meter_->OnTransferStart();  // 1 transfer
  test_clock_->Advance(kElapsed1);

  meter_->OnTransferStart();  // 2 transfers

  meter_->OnBytesTransferred(kBytes1);

  {
    InSequence sequence;
    EXPECT_CALL(*averager_, AddSample(kWeight1, kBitrate1)).Times(1);
    EXPECT_CALL(*averager_, GetAverage()).WillRepeatedly(Return(kResult1));
  }
  EXPECT_CALL(*sample_receiver_,
              BandwidthSampleCallback(kElapsed1, kBytes1, kResult1));
  meter_->OnTransferEnd();  // 1 transfer

  {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  VerifyAndClearMocks();

  const base::TimeDelta kElapsed2 = base::TimeDelta::FromSeconds(36);
  constexpr int64_t kBytes2a = 600;
  constexpr int64_t kBytes2b = 2000;
  constexpr int64_t kBytes2c = 1000;
  constexpr int64_t kBytes2Total = 3600;
  constexpr int64_t kWeight2 = 60;
  constexpr int64_t kBitrate2 = 800;
  constexpr int64_t kResult2 = 543210;

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  meter_->OnBytesTransferred(kBytes2a);
  meter_->OnBytesTransferred(kBytes2b);
  meter_->OnBytesTransferred(kBytes2c);

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  test_clock_->Advance(kElapsed2);

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult1));

  {
    InSequence sequence;
    EXPECT_CALL(*averager_, AddSample(kWeight2, kBitrate2)).Times(1);
    EXPECT_CALL(*averager_, GetAverage()).WillRepeatedly(Return(kResult2));
  }
  EXPECT_CALL(*sample_receiver_,
              BandwidthSampleCallback(kElapsed2, kBytes2Total, kResult2));
  meter_->OnTransferEnd();  // 0 transfers

  {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  EXPECT_THAT(meter_->GetBitrateEstimate(), Eq(kResult2));
}

}  // namespace upstream
}  // namespace ndash
