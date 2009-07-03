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
// Declares the interface to in-memory metrics capture
#ifndef OMAHA_STATSREPORT_METRICS_H__
#define OMAHA_STATSREPORT_METRICS_H__

#include <iterator>
#include "base/basictypes.h"
#include "omaha/common/highres_timer-win32.h"
#include "omaha/common/logging/logging.h"

/// Macros to declare & define named & typed metrics.
/// Put declarations in headers or in cpp files, where you need access
/// to the metrics. For each declared metric, there must be precisely
/// one definition in a compilation unit someplace.

/// A count metric should be used to report anything that monotonically
/// increases.
/// Examples:
///    # event count
///      how often does this condition hit, this function get called
///    # aggregate sums
///      how many bytes are written
#define DECLARE_METRIC_count(name)   DECLARE_METRIC(CountMetric, name)
#define DEFINE_METRIC_count(name)   DEFINE_METRIC(CountMetric, name)

/// Use timing metrics to report on the performance of important things.
/// A timing metric will report the count of occurrences, as well as the
/// average, min and max times.
/// Samples are measured in milliseconds if you use the TIME_SCOPE macro
/// or the HighResTimer class to collect samples.
#define DECLARE_METRIC_timing(name)  DECLARE_METRIC(TimingMetric, name)
#define DEFINE_METRIC_timing(name)  DEFINE_METRIC(TimingMetric, name)

/// Collects a sample from here to the end of the current scope, and
/// adds the sample to the timing metric supplied
#define TIME_SCOPE(timing) \
  stats_report::TimingSample __xxsample__(timing)

/// Use integer metrics to report runtime values that fluctuate.
/// Examples:
///    # object count
///      How many objects of some type exist
///    # disk space or memory
///      How much disk space or memory is in use
#define DECLARE_METRIC_integer(name) DECLARE_METRIC(IntegerMetric, name)
#define DEFINE_METRIC_integer(name) DEFINE_METRIC(IntegerMetric, name)


/// Use boolean metrics to report the occurrence of important but rare events
/// or conditions. Note that a boolean metric is tri-state, so you typically
/// want to set it only in one direction, and typically to true.
/// Setting a boolean metric one way or another on a trigger event will report
/// the setting of the boolean immediately prior to reporting, which is
/// typically not what you want.
#define DECLARE_METRIC_bool(name)    DECLARE_METRIC(BoolMetric, name)
#define DEFINE_METRIC_bool(name)    DEFINE_METRIC(BoolMetric, name)


/// Implementation macros
#define DECLARE_METRIC(type, name) \
  namespace omaha_client_statsreport { \
  extern stats_report::type metric_##name; \
  } \
  using omaha_client_statsreport::metric_##name

#define DEFINE_METRIC(type, name) \
  namespace omaha_client_statsreport { \
  stats_report::type metric_##name(#name, \
  &stats_report::g_global_metric_storage); \
  } \
  using omaha_client_statsreport::metric_##name


namespace stats_report {

enum MetricType {
  // use zero for invalid, because global storage defaults to zero
  kInvalidType = 0,
  kCountType,
  kTimingType,
  kIntegerType,
  kBoolType
};

// fwd.
struct MetricCollectionBase;
class MetricCollection;
class MetricBase;
class IntegerMetricBase;
class CountMetric;
class TimingMetric;
class IntegerMetric;
class BoolMetric;

/// Base class for all stats instances.
/// Stats instances are chained together against a MetricCollection to
/// allow enumerating stats.
///
/// MetricCollection is factored into a class to make it easier to unittest
/// the implementation.
class MetricBase {
public:
  /// @name Downcasts
  /// @{
  CountMetric &AsCount();
  TimingMetric &AsTiming();
  IntegerMetric &AsInteger();
  BoolMetric &AsBool();

  const CountMetric &AsCount() const;
  const TimingMetric &AsTiming() const;
  const IntegerMetric &AsInteger() const;
  const BoolMetric &AsBool() const;
  /// @}

  /// @name Accessors
  /// @{
  MetricType type() const { return type_; }
  MetricBase *next() const { return next_; }
  const char *name() const { return name_; }
  /// @}

  // TODO(omaha): does this need to be virtual?
  virtual ~MetricBase() = 0;

protected:
  class ObjectLock;
  void Lock() const;
  void Unlock() const;

  /// Constructs a MetricBase and adds to the provided MetricCollection.
  /// @note Metrics can only be constructed up to the point where the
  ///     MetricCollection is initialized, and there's no locking performed.
  ///     The assumption is that outside unit tests, Metrics will we declared
  ///     as static/global variables, and initialized at static initialization
  ///     time - and static initialization is single-threaded.
  MetricBase(const char *name, MetricType type, MetricCollectionBase *coll);

  /// Constructs a named typed MetricBase
  MetricBase(const char *name, MetricType type);

  /// Our name
  char const *const name_;

  /// type of this metric
  MetricType const type_;

  /// chains to next stat instance
  MetricBase *const next_;

  /// The collection we're created against
  MetricCollectionBase *const coll_;

private:
  DISALLOW_EVIL_CONSTRUCTORS(MetricBase);
};

/// Must be a POD
struct MetricCollectionBase {
  bool initialized_;
  MetricBase *first_;
};

/// Inherit from base, which is a POD and can be initialized at link time.
///
/// The global MetricCollection is aliased to a link-time initialized
/// instance of MetricCollectionBase, and must not extend the size of its
/// base class.
class MetricCollection: public MetricCollectionBase {
public:
  MetricCollection() {
    initialized_ = false;
    first_ = NULL;
  }
  ~MetricCollection() {
    DCHECK(NULL == first_);
  }

  /// Initialize must be called after all metrics have been added to the
  /// collection, but before enumerating it for e.g. aggregation or reporting.
  /// The intent is that outside unit tests, there will only be the global
  /// metrics collection, which will accrue all metrics defined with the
  /// DEFINE_METRIC_* macros.
  /// Typically you'd call Initialize very early in your main function, and
  /// Uninitialize towards the end of main.
  /// It is an error to Initialize() when the collection is initialized().
  void Initialize();

  /// Uninitialize must be called before removing (deleting or deconstructing)
  /// metrics from the collection.
  /// It is an error to Uninitialize() when the collection is !initialized().
  void Uninitialize();

  MetricBase *first() const { return first_; }
  bool initialized() const { return initialized_; }

private:
  using MetricCollectionBase::initialized_;
  using MetricCollectionBase::first_;

  DISALLOW_EVIL_CONSTRUCTORS(MetricCollection);

  /// MetricBase is intimate with us
  friend class MetricBase;
};

/// Implements a forward_iterator for MetricCollection.
class MetricIterator: public std::iterator<std::forward_iterator_tag,
                                           MetricBase *> {
public:
  MetricIterator() : curr_(NULL) {
  }
  MetricIterator(const MetricIterator &other) : curr_(other.curr_) {
  }
  MetricIterator(const MetricCollection &coll) : curr_(coll.first()) {
    DCHECK(coll.initialized());
  }

  MetricBase *operator*() const {
    return curr_;
  }
  MetricBase *operator->() const {
    return curr_;
  }
  MetricIterator operator++() { // preincrement
    if (curr_)
      curr_ = curr_->next();

    return (*this);
  }
  MetricIterator operator++(int) {// postincrement
    MetricIterator ret = *this;
    ++*this;
    return (ret);
  }

private:
  MetricBase *curr_;
};

inline bool operator == (const MetricIterator &a, const MetricIterator &b) {
  return *a == *b;
}
inline bool operator != (const MetricIterator &a, const MetricIterator &b) {
  return !operator == (a, b);
}

/// Globally defined counters are registered here
extern MetricCollectionBase g_global_metric_storage;

/// And more conveniently accessed through here
extern MetricCollection &g_global_metrics;

/// Base class for integer metrics
class IntegerMetricBase: public MetricBase {
public:
  /// Sets the current value
  void Set(uint64 value);

  /// Retrieves the current value
  uint64 value() const;

  void operator ++ ()     { Increment(); }
  void operator ++ (int)  { Increment(); }
  void operator += (uint64 addend) { Add(addend); }

protected:
  IntegerMetricBase(const char *name,
                    MetricType type,
                    MetricCollectionBase *coll)
      : MetricBase(name, type, coll), value_(0) {
  }
  IntegerMetricBase(const char *name, MetricType type, uint64 value)
      : MetricBase(name, type), value_(value) {
  }

  void Increment();
  void Decrement();
  void Add(uint64 value);
  void Subtract(uint64 value);

  uint64 value_;

private:
  DISALLOW_EVIL_CONSTRUCTORS(IntegerMetricBase);
};

/// A count metric is a cumulative counter of events.
class CountMetric: public IntegerMetricBase {
public:
  CountMetric(const char *name, MetricCollectionBase *coll)
      : IntegerMetricBase(name, kCountType, coll) {
  }

  CountMetric(const char *name, uint64 value)
      : IntegerMetricBase(name, kCountType, value) {
  }

  /// Nulls the metric and returns the current values.
  uint64 Reset();

private:
  DISALLOW_EVIL_CONSTRUCTORS(CountMetric);
};

class TimingMetric: public MetricBase {
public:
  struct TimingData {
    uint32 count;
    uint32 align; // allow access to the alignment gap between count and sum,
                  // makes it esier to unittest.
    uint64 sum; // ms
    uint64 minimum; // ms
    uint64 maximum; // ms
  };

  TimingMetric(const char *name, MetricCollectionBase *coll)
      : MetricBase(name, kTimingType, coll) {
    Clear();
  }

  TimingMetric(const char *name, const TimingData &value)
      : MetricBase(name, kTimingType), data_(value) {
  }

  uint32 count() const;
  uint64 sum() const;
  uint64 minimum() const;
  uint64 maximum() const;
  uint64 average() const;

  /// Adds a single sample to the metric
  /// @param time_ms time (in milliseconds) for this sample
  void AddSample(uint64 time_ms);

  /// Adds count samples to the metric
  /// @note use this when capturing time over a variable number of items to
  ///     normalize e.g. download time per byte or KB. This records one sample
  ///     over count items, which is numerically more stable for the average
  ///     than dividing the captured time by the item count. As a side benefit
  ///     the timer will also record the item count.
  /// @note if count == 0, no sample will be recorded
  /// @param count number of samples to add
  /// @param total_time_ms the total time consumed by all the "count" samples
  void AddSamples(uint64 count, uint64 total_time_ms);

  /// Nulls the metric and returns the current values.
  TimingData Reset();

private:
  DISALLOW_EVIL_CONSTRUCTORS(TimingMetric);

  void Clear();

  TimingData data_;
};

/// A convenience class to sample the time from construction to destruction
/// against a given timing metric.
class TimingSample {
public:
  /// @param timing the metric the sample is to be tallied against
  explicit TimingSample(TimingMetric &timing) : timing_(timing), count_(1) {
  }

  /// @param timing the metric the sample is to be tallied against
  /// @param item_count count of items processed, used to divide the sampled
  ///     time so as to capture time per item, which is often a better measure
  ///     than the total time over a varying number of items.
  TimingSample(TimingMetric &timing, uint32 item_count) : timing_(timing),
      count_(item_count) {
  }

  ~TimingSample() {
    // We discard samples with a zero count
    if(count_ == 1)
      timing_.AddSample(timer_.GetElapsedMs());
    else
      timing_.AddSamples(count_, timer_.GetElapsedMs());
  }

  /// @name Accessors
  /// @{
  uint32 count() const { return count_; }
  void set_count(uint32 count) { count_ = count; }
  /// @}

private:
  /// Collects the sample for us.
  omaha::HighresTimer timer_;

  /// The metric we tally against.
  TimingMetric &timing_;

  /// The item count we divide the captured time by
  uint32 count_;

  DISALLOW_EVIL_CONSTRUCTORS(TimingSample);
};

/// An integer metric is used to sample values that vary over time.
/// On aggregation the instantaneous value of the integer metric is captured.
class IntegerMetric: public IntegerMetricBase {
public:
  IntegerMetric(const char *name, MetricCollectionBase *coll)
      : IntegerMetricBase(name, kIntegerType, coll) {
  }

  IntegerMetric(const char *name, uint64 value)
      : IntegerMetricBase(name, kIntegerType, value) {
  }

  void operator = (uint64 value)   { Set(value); }

  void operator -- ()     { Decrement(); }
  void operator -- (int)  { Decrement(); }
  void operator -= (uint64 sub)    { Subtract(sub); }

private:
  DISALLOW_EVIL_CONSTRUCTORS(IntegerMetric);
};

/// A bool metric is tri-state, and can be:
///    - unset,
///    - true or
///    - false
/// to match other metrics, which are implicitly unset if they've not changed
/// from their initial value.
class BoolMetric: public MetricBase {
public:
  /// Values we can take
  enum TristateBoolValue {
    kBoolUnset = -1,
    kBoolFalse,
    kBoolTrue,
  };

  BoolMetric(const char *name, MetricCollectionBase *coll)
        : MetricBase(name, kBoolType, coll), value_(kBoolUnset) {
  }

  BoolMetric(const char *name, uint32 value)
        : MetricBase(name, kBoolType) {
    switch (value) {
     case kBoolFalse:
     case kBoolTrue:
      value_ = static_cast<TristateBoolValue>(value);
      break;

     default:
      DCHECK(false && "Unexpected tristate bool value on construction");
      value_ = kBoolUnset;
    }
  }

  /// Sets the flag to the provided value.
  void Set(bool value);

  void operator = (bool value) {
    Set(value);
  }

  /// Nulls the metric and returns the current values.
  TristateBoolValue Reset();

  /// Returns the current value - not threadsafe
  TristateBoolValue value() const { return value_; };

private:
  DISALLOW_EVIL_CONSTRUCTORS(BoolMetric);

  TristateBoolValue value_;
};

inline CountMetric &MetricBase::AsCount() {
  DCHECK_EQ(kCountType, type());

  return static_cast<CountMetric&>(*this);
}

inline TimingMetric &MetricBase::AsTiming() {
  DCHECK_EQ(kTimingType, type());

  return static_cast<TimingMetric&>(*this);
}

inline IntegerMetric &MetricBase::AsInteger() {
  DCHECK_EQ(kIntegerType, type());

  return static_cast<IntegerMetric&>(*this);
}

inline BoolMetric &MetricBase::AsBool() {
  DCHECK_EQ(kBoolType, type());

  return static_cast<BoolMetric&>(*this);
}

inline const CountMetric &MetricBase::AsCount() const {
  DCHECK_EQ(kCountType, type());

  return static_cast<const CountMetric&>(*this);
}

inline const TimingMetric &MetricBase::AsTiming() const {
  DCHECK_EQ(kTimingType, type());

  return static_cast<const TimingMetric&>(*this);
}

inline const IntegerMetric &MetricBase::AsInteger() const {
  DCHECK_EQ(kIntegerType, type());

  return static_cast<const IntegerMetric&>(*this);
}

inline const BoolMetric &MetricBase::AsBool() const {
  DCHECK_EQ(kBoolType, type());

  return static_cast<const BoolMetric&>(*this);
}

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_METRICS_H__
