// Copyright 2006-2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================
//
// Implements metrics and metrics collections
#include "omaha/statsreport/metrics.h"
#include "omaha/common/debug.h"
#include "omaha/common/synchronized.h"

namespace stats_report {
// Make sure global stats collection is placed in zeroed storage so as to avoid
// initialization order snafus.
MetricCollectionBase g_global_metric_storage = { 0, 0 };
MetricCollection &g_global_metrics =
                  *static_cast<MetricCollection*>(&g_global_metric_storage);

#pragma warning(push)
// C4640: construction of local static object is not thread-safe.
// C4073: initializers put in library initialization area.
#pragma warning(disable : 4640 4073)

// Serialize all metric manipulation and access under this lock.
//
// Initializes g_lock before other global objects of user defined types.
// It assumes the program is single threaded while executing CRT startup and
// exit code.
#pragma init_seg(lib)
omaha::LLock g_lock;
#pragma warning(pop)

class MetricBase::ObjectLock {
public:
  ObjectLock(const MetricBase *metric) : metric_(metric) {
    metric_->Lock();
  }

  ~ObjectLock() {
    metric_->Unlock();
  }

private:
  MetricBase const *const metric_;
  DISALLOW_EVIL_CONSTRUCTORS(MetricBase::ObjectLock);
};

void MetricBase::Lock() const {
  g_lock.Lock();
}

void MetricBase::Unlock() const {
  g_lock.Unlock();
}

MetricBase::MetricBase(const char *name,
                       MetricType type,
                       MetricCollectionBase *coll)
    : name_(name), type_(type), next_(coll->first_), coll_(coll) {
  DCHECK_NE(static_cast<MetricCollectionBase*>(NULL), coll_);
  DCHECK_EQ(false, coll_->initialized_);
  coll->first_ = this;
}

MetricBase::MetricBase(const char *name, MetricType type)
    : name_(name), type_(type), next_(NULL), coll_(NULL) {
}

MetricBase::~MetricBase() {
  if (coll_) {
    DCHECK_EQ(this, coll_->first_);
    DCHECK(!coll_->initialized_)
      << "Metric destructor called without call to Uninitialize().";

    coll_->first_ = next_;
  } else {
    DCHECK(NULL == next_);
  }
}

void IntegerMetricBase::Set(uint64 value) {
  ObjectLock lock(this);
  ASSERT1(value != kint64max);
  value_ = value;
}

uint64 IntegerMetricBase::value() const {
  ObjectLock lock(this);
  uint64 ret = value_;
  ASSERT1(ret != kint64max);
  return ret;
}

void IntegerMetricBase::Increment() {
  ObjectLock lock(this);
  ++value_;
}

void IntegerMetricBase::Decrement() {
  ObjectLock lock(this);
  --value_;
}

void IntegerMetricBase::Add(uint64 value){
  ObjectLock lock(this);
  value_ += value;
}

void IntegerMetricBase::Subtract(uint64 value) {
  ObjectLock lock(this);
  if (value_ < value)
    value_ = 0;
  else
    value_ -= value;
}

uint64 CountMetric::Reset() {
  ObjectLock lock(this);
  uint64 ret = value_;
  value_ = 0;
  return ret;
}

TimingMetric::TimingData TimingMetric::Reset() {
  ObjectLock lock(this);
  TimingData ret = data_;
  Clear();
  return ret;
}

uint32 TimingMetric::count() const {
  ObjectLock lock(this);
  uint32 ret = data_.count;
  return ret;
}

uint64 TimingMetric::sum() const {
  ObjectLock lock(this);
  uint64 ret = data_.sum;
  return ret;
}

uint64 TimingMetric::minimum() const {
  ObjectLock lock(this);
  uint64 ret = data_.minimum;
  return ret;
}

uint64 TimingMetric::maximum() const {
  ObjectLock lock(this);
  uint64 ret = data_.maximum;
  return ret;
}

uint64 TimingMetric::average() const {
  ObjectLock lock(this);

  uint64 ret = 0;
  if (0 == data_.count) {
    DCHECK_EQ(0, data_.sum);
  } else {
    ret = data_.sum / data_.count;
  }
  return ret;
}

void TimingMetric::AddSample(uint64 time_ms) {
  ObjectLock lock(this);
  if (0 == data_.count) {
    data_.minimum = time_ms;
    data_.maximum = time_ms;
  } else {
    if (data_.minimum > time_ms)
      data_.minimum = time_ms;
    if (data_.maximum < time_ms)
      data_.maximum = time_ms;
  }
  data_.count++;
  data_.sum += time_ms;
}

void TimingMetric::AddSamples(uint64 count, uint64 total_time_ms) {
  if (0 == count)
    return;

  uint64 time_ms = total_time_ms / count;

  ObjectLock lock(this);
  if (0 == data_.count) {
    data_.minimum = time_ms;
    data_.maximum = time_ms;
  } else {
    if (data_.minimum > time_ms)
      data_.minimum = time_ms;
    if (data_.maximum < time_ms)
      data_.maximum = time_ms;
  }

  // TODO(omaha): truncation from 64 to 32 may occur here.
  DCHECK_LE(count, kuint32max);
  data_.count += static_cast<uint32>(count);
  data_.sum += total_time_ms;
}

void TimingMetric::Clear() {
  memset(&data_, 0, sizeof(data_));
}

void BoolMetric::Set(bool value) {
  ObjectLock lock(this);
  value_ = value ? kBoolTrue : kBoolFalse;
}

BoolMetric::TristateBoolValue BoolMetric::Reset() {
  ObjectLock lock(this);
  TristateBoolValue ret = value_;
  value_ = kBoolUnset;
  return ret;
}

void MetricCollection::Initialize() {
  DCHECK(!initialized());
  initialized_ = true;
}

void MetricCollection::Uninitialize() {
  DCHECK(initialized());
  initialized_ = false;
}


}  // namespace stats_report
