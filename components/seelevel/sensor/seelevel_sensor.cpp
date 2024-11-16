#include "seelevel_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
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
    ESP_LOGCONFIG(TAG, "    invert %s", this->volume_invert_ ? "true" : "false");
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
  // its proximity to the air/water boundary.
  //
  // - The sensor is sensitive to noise.
  // - The sensor response curve is unknown and varies over time.
  // - Some sensor segments appear to be more sensitive than others, particularly
  //   near boundaries such as the top of the tank.
  //
  // Refer to decode.py for explanations and experiments.
#if 0 // stepwise estimator
  constexpr unsigned thresh = 120;
  for (unsigned i = 0; i < segments_; i++) {
    unsigned x = data[segments_ - i - 1];
    if (x < thresh) return i;
  }
  return segments_;
#elif 0 // proportional estimator
  unsigned high = 0;
  unsigned low = 255;
  for (unsigned i = 0; i < segments_; i++) {
    unsigned x = data[segments_ - i - 1];
    if (x > high) high = x;
    if (x < low) low = x;
  }
  if (high < 120) high = 120;
  if (high > 200) high = 200;
  if (low > high / 4) low = high / 4;
  if (low < 20) low = 20;
  unsigned thresh = low + (high - low) * 6 / 10;
  for (unsigned i = 0; i < segments_; i++) {
    unsigned x = data[segments_ - i - 1];
    if (x < low) return i;
    if (x < thresh) {
      return std::min(std::round(float(x - low) / (thresh - low) * 10.f) / 10.f, 1.f) + i;
    }
  }
  return segments_;
#else // boundary estimator
  unsigned thresh = 120;
  for (unsigned i = 0; i < segments_; i++) {
    unsigned x = data[segments_ - i - 1];
    if (x < thresh) {
      unsigned low = thresh / 3;
      if (x < low) return i;
      return std::round(float(x - low) / float(thresh - low) * 10.f) / 10.f + i;
    }
    thresh = std::max(thresh, x * 9 / 10);
  }
  return segments_;
#endif
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
  float volume = lerp((level - low_level) / (high_level - low_level), low_volume, high_volume);
  return this->volume_invert_ ? this->volume_mappings_.back().second - volume : volume;
}

}  // namespace seelevel
}  // namespace esphome
