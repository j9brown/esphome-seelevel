#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "seelevel.h"

#include <utility>

namespace esphome {
namespace seelevel {

static const char *const TAG = "Seelevel";

float SeelevelComponent::get_setup_priority() const { return setup_priority::HARDWARE; }

void SeelevelComponent::setup() {
  this->rx_pin_->setup();
  this->tx_pin_->setup();
  this->tx_pin_->digital_write(false);
}

void SeelevelComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Seelevel component:");
  ESP_LOGCONFIG(TAG, "  RX Pin: %s", this->rx_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "  TX Pin: %s", this->tx_pin_->dump_summary().c_str());
}

float SeelevelComponent::read_tank(unsigned tank, unsigned segments) {
  if (!this->rx_pin_->digital_read()) {
    ESP_LOGW(TAG, "The sensor interface is malfunctioning: RX is low while TX is low");
    return NAN;
  }

  this->tx_pin_->digital_write(true);
  delay(3); // charge the sensor, exact timing not important

  float level = this->read_tank_with_tx_active_(tank, segments);

  this->tx_pin_->digital_write(false);
  return level;
}

float SeelevelComponent::read_tank_with_tx_active_(unsigned tank, unsigned segments) {
  if (this->rx_pin_->digital_read()) {
    ESP_LOGW(TAG, "The sensor wiring short-circuits to ground");
    return NAN;
  }

  // Send the sensor address.
  for (unsigned i = 0; i < tank; i++) {
    this->tx_pin_->digital_write(false);
    delayMicroseconds(85);
    this->tx_pin_->digital_write(true);
    delayMicroseconds(300);
  }

  // Wait for a response.
  uint32_t timeout = 20000; // long timeout for first bit
  uint8_t packet[12];
  for (size_t i = 0; i < sizeof(packet); i++) {
    uint8_t byte = 0;
    for (size_t j = 0; j < 8; j++) {
      uint32_t start = micros();
      uint32_t end = start;
      while (!this->rx_pin_->digital_read()) {
        end = micros();
        if (end - start > timeout) {
          if (i == 0 && j == 0) {
            ESP_LOGW(TAG, "No response from tank %d after %d microseconds", tank, end - start);
          } else {
            ESP_LOGW(TAG, "Timeout while reading data from tank %d after %d microseconds", tank, end - start);
          }
          return NAN;
        }
      }
      start = end;
      timeout = 200; // shorter timeout for subsequent bits
      while (this->rx_pin_->digital_read()) {
        end = micros();
        if (end - start > timeout) {
          ESP_LOGW(TAG, "Timeout while reading data from tank %d after %d microseconds", tank, end - start);
          return NAN;
        }
      }
      byte <<= 1;
      if (end - start > 26) byte |= 1;
    }
    packet[i] = byte;
  }

  ESP_LOGV(TAG, "Tank %d sensor packet: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x", tank,
      packet[0], packet[1], packet[2], packet[3], packet[4], packet[5],
      packet[6], packet[7], packet[8], packet[9], packet[10], packet[11]);

  // Decode the packet header.
  // - The first 4 bits of the packet contain unknown information. It seems to contain the address
  //   of the tank sender. The high bit is always set (for my sensors) and perhaps it is used to
  //   indicate whether the top vs. bottom sensor is responding.
  //   - Observed 0x9 when addressing tank #1 bottom sensor
  //   - Observed 0xA when addressing tank #2 bottom sensor
  //   - Speculate that we'd see 0x1 when addressing tank #1 top sensor
  // - The next 12 bits of the packet contain a simple arithmetic checksum of the remaining bytes.
  uint16_t check_sum = ((packet[0] << 8) | packet[1]) & 0x0fff;

  // Check the checksum.
  uint16_t actual_sum = 0;
  for (size_t i = 0; i < 10; i++) {
    actual_sum += packet[i + 2];
  }
  if (check_sum != actual_sum) {
    ESP_LOGW(TAG, "Checksum mismatch while reading data from tank %d, expected %04x, actual %04x, difference %04x",
        tank, check_sum, actual_sum, (actual_sum - check_sum) & 0xffff);
    return NAN;
  }

  // Calculate the level assuming that the zeroeth segment is at the top of the tank.
  // The range of readings varies depending on how the sensor has been installed and is
  // sensitive to nearby electric fields. Each segment on the sensor is influenced by
  // nearby liquid some distance away from the capacitive plate so they cannot be
  // interpreted in a simple binary manner.
  //
  // We make the following assumptions:
  //
  // - The calculation should yield a linear response to changes in volume without stair steps.
  // - The sensor is sensitive to noise, low confidence values can be discarded.
  // - The sensor response curve is unknown and may change over time.
  // - We can use the ratio of adjacent sensor segments to normalize the response.
  // - Shallower sensor segments tend to respond more weakly than deeper ones.
  constexpr unsigned noise_threshold = 10;
  constexpr unsigned baseline = 160;
  constexpr float scale = 0.92f;

  const uint8_t* data = packet + 2;
  ESP_LOGD(TAG, "Tank %d sensor data: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d", tank,
      data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9]);

  float level = 0.f;
  unsigned prior = baseline;
  for (unsigned i = 0; i < segments; i++) {
    unsigned value = data[segments - i - 1];
    if (value < noise_threshold) break;
    float contribution = value / (prior * scale);
    level += std::min(contribution, i == segments - 1 ? 1.f / scale : 1.f);
    prior = value;
  }
  return level;
}

}  // namespace seelevel
}  // namespace esphome
