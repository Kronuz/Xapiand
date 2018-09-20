#pragma once

#include <map>
#include <memory>
#include <mutex>

#include "client_metric.h"
#include "collectable.h"
#include "counter_builder.h"
#include "family.h"
#include "gauge_builder.h"
#include "histogram.h"
#include "histogram_builder.h"
#include "summary.h"
#include "summary_builder.h"

namespace prometheus {

class Registry : public Collectable {
 public:
  friend class detail::CounterBuilder;
  friend class detail::GaugeBuilder;
  friend class detail::HistogramBuilder;
  friend class detail::SummaryBuilder;

  // collectable
  std::vector<MetricFamily> Collect() override;

 private:
  Family<Counter>& AddCounter(const std::string& name, const std::string& help,
                              const std::map<std::string, std::string>& labels);
  Family<Gauge>& AddGauge(const std::string& name, const std::string& help,
                          const std::map<std::string, std::string>& labels);
  Family<Histogram>& AddHistogram(
      const std::string& name, const std::string& help,
      const std::map<std::string, std::string>& labels);
  Family<Summary>& AddSummary(const std::string& name, const std::string& help,
                              const std::map<std::string, std::string>& labels);

  std::vector<std::unique_ptr<Collectable>> collectables_;
  std::mutex mutex_;
};
}
