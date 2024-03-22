#pragma once

#include "../seelevel.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <vector>
#include <utility>

namespace esphome {
namespace seelevel {

class SeelevelSensor : public PollingComponent {
 public:
  SeelevelSensor(SeelevelComponent *parent) : parent_(parent) {}

  float get_setup_priority() const override;
  void dump_config() override;
  void update() override;

  void set_tank(unsigned tank) { this->tank_ = tank; }
  void set_segments(unsigned segments) { this->segments_ = segments; }
  void set_level_sensor(sensor::Sensor *sensor) { this->level_sensor_ = sensor; }
  void set_volume_sensor(sensor::Sensor *sensor) { this->volume_sensor_ = sensor; }
  void append_volume_mapping(float level, float volume) { this->volume_mappings_.push_back(std::make_pair(level, volume)); }

 protected:
  float estimate_volume(float level) const;

  SeelevelComponent *parent_;
  unsigned tank_{};
  unsigned segments_{};
  sensor::Sensor *level_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};
  std::vector<std::pair<float, float>> volume_mappings_{{0., 0.}};
};

}  // namespace seelevel
}  // namespace esphome
