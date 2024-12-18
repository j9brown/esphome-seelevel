#pragma once

#include "../seelevel.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
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
  void set_volume_invert(bool invert) { this->volume_invert_ = invert; }
  void set_segment_data_text_sensor(text_sensor::TextSensor *text_sensor) { this->segment_data_text_sensor_ = text_sensor; }

 protected:
  float estimate_level(const SeelevelComponent::SegmentData& data) const;
  float estimate_volume(float level) const;

  SeelevelComponent *parent_;
  unsigned tank_{};
  unsigned segments_{};
  sensor::Sensor *level_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};
  text_sensor::TextSensor *segment_data_text_sensor_{nullptr};
  std::vector<std::pair<float, float>> volume_mappings_{};
  bool volume_invert_{};
};

}  // namespace seelevel
}  // namespace esphome
