#pragma once

#include <string>
#include <vector>

#include "client_metric.h"
#include "metric_type.h"

namespace prometheus {

struct MetricFamily {
  std::string name;
  std::string help;
  MetricType type = MetricType::Untyped;
  std::vector<ClientMetric> metric;
};
}  // namespace prometheus
