#pragma once

#include <map>
#include <memory>
#include <mutex>

#include "client_metric.h"
#include "collectable.h"
#include "family.h"
#include "histogram.h"
#include "summary.h"

namespace prometheus {

class Registry : public Collectable {
 public:
  // collectable
  std::vector<MetricFamily> Collect() override;

  Family<Counter>& AddCounter(const std::string& name, const std::string& help,
                              const std::map<std::string, std::string>& constant_labels);
  Family<Gauge>& AddGauge(const std::string& name, const std::string& help,
                          const std::map<std::string, std::string>& constant_labels);
  Family<Histogram>& AddHistogram(
      const std::string& name, const std::string& help,
      const std::map<std::string, std::string>& constant_labels);
  Family<Summary>& AddSummary(const std::string& name, const std::string& help,
                              const std::map<std::string, std::string>& constant_labels);

 private:
  std::vector<std::unique_ptr<Collectable>> collectables_;
  std::mutex mutex_;
};
}
