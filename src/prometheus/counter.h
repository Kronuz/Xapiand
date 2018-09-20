#pragma once

#include <atomic>

#include "client_metric.h"
#include "gauge.h"
#include "metric.h"

namespace prometheus {
class Counter {
 public:
  static const MetricType metric_type = MetricType::Counter;

  void Increment();
  void Increment(double);
  double Value() const;

  ClientMetric Collect();

 private:
  Gauge gauge_;
};
}
