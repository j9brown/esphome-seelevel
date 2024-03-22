#include "seelevel_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace seelevel {

static const char *const TAG = "Seelevel.sensor";

float SeelevelSensor::get_setup_priority() const { return setup_priority::DATA; }

void SeelevelSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Seelevel sensor:");
  LOG_SENSOR("  ", "Level", this->level_sensor_);
  LOG_SENSOR("  ", "Volume", this->volume_sensor_);
  if (volume_sensor_) {
    for (const auto& mapping : this->volume_mappings_) {
      ESP_LOGCONFIG(TAG, "    level %f -> volume %f", mapping.first, mapping.second);
    }
  }
}

void SeelevelSensor::update() {
  float level = this->parent_->read_tank(this->tank_, this->segments_);
  if (this->level_sensor_) {
    this->level_sensor_->publish_state(level);
  }

  if (this->volume_sensor_) {
    this->volume_sensor_->publish_state(this->estimate_volume(level));
  }
}

float SeelevelSensor::estimate_volume(float level) const {
  if (std::isnan(level)) return NAN;

  float low_level = 0;
  float low_volume = 0;
  float high_level = 0;
  float high_volume = 0;
  for (const auto& mapping : this->volume_mappings_) {
    high_level = mapping.first;
    high_volume = mapping.second;
    if (level < high_level) break;
  }
  if (level >= high_level || low_level >= high_level) return high_volume;
  return lerp((level - low_level) / (high_level - low_level), low_volume, high_volume);
}

}  // namespace seelevel
}  // namespace esphome
