#include "seelevel_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <sstream>

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
  LOG_TEXT_SENSOR("  ", "Segment Data", this->segment_data_text_sensor_);
}

void SeelevelSensor::update() {
  SeelevelComponent::SegmentData segment_data;
  float level = NAN;
  if (this->parent_->read_tank(this->tank_, &segment_data)) {
    if (this->segment_data_text_sensor_) {
      std::ostringstream segment_state;
      for (unsigned i = 0; i < segments_; i++) {
        unsigned value = segment_data[segments_ - i - 1];
        if (i != 0) {
          segment_state << ',';
        }
        segment_state << value;
      }
      this->segment_data_text_sensor_->publish_state(segment_state.str());
    }
    level = estimate_level(segment_data);
  }

  if (this->level_sensor_) {
    this->level_sensor_->publish_state(level);
  }

  if (this->volume_sensor_) {
    this->volume_sensor_->publish_state(this->estimate_volume(level));
  }
}

float SeelevelSensor::estimate_level(const SeelevelComponent::SegmentData& data) const {
  // Calculate the level assuming that the zeroeth segment is at the top of the tank.
  // The range of readings varies depending on how the sensor has been installed and is
  // sensitive to nearby electric fields. Each segment on the sensor is influenced by
  // nearby liquid some distance away from the capacitive plate.
  //
  // - The sensor is sensitive to noise.
  // - The sensor response curve is unknown and varies over time.
  // - Some sensor segments appear to be more sensitive than others.
  //
  // TODO: With a better algorithm, it should be possible to interpolate the sensor
  // readings to obtain a more linear response.
  constexpr unsigned noise_floor = 180;
  for (unsigned i = 0; i < segments_; i++) {
    unsigned value = data[segments_ - i - 1];
    if (value < noise_floor) return i;
  }
  return segments_;
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
