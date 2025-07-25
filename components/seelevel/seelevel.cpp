#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "seelevel.h"

#include <algorithm>
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

SeelevelComponent::ReadResult SeelevelComponent::read_tank(unsigned tank, SegmentData* out_data) {
  // Need to wait between consecutive reads otherwise the sensor fails to respond.
  constexpr uint32_t MIN_INVERVAL_BETWEEN_READS = 1000; // at least 800 ms
  if (millis() - this->last_read_time_ < MIN_INVERVAL_BETWEEN_READS) {
    return ReadResult::RATE_LIMITED;
  }

  if (!this->rx_pin_->digital_read()) {
    ESP_LOGW(TAG, "The sensor interface is malfunctioning: RX is low while TX is low");
    this->last_read_time_ = millis();
    return ReadResult::FAILED;
  }

  // Charge the sensor
  // Apparently this needs to be a fairly short interval
  // - 1.5 ms: always get a response
  // - 2 ms: always get a response
  // - 3 ms: usually get a response, sometimes don't, seems to depend on environmental conditions
  // - 5 ms: sometimes get a response
  // - 10 ms: no response
  constexpr uint32_t CHARGE_TIME = 1500;
  this->tx_pin_->digital_write(true);
  delayMicroseconds(CHARGE_TIME);

  bool result = this->read_tank_with_tx_active_(tank, out_data);

  this->tx_pin_->digital_write(false);
  this->last_read_time_ = millis();
  return result ? ReadResult::OK : ReadResult::FAILED;
}

bool SeelevelComponent::read_tank_with_tx_active_(unsigned tank, SegmentData* out_data) {
  if (this->rx_pin_->digital_read()) {
    ESP_LOGW(TAG, "The sensor wiring short-circuits to ground");
    return false;
  }

  // Send the sensor address.
  for (unsigned i = 0; i < tank; i++) {
    this->tx_pin_->digital_write(false);
    delayMicroseconds(85);
    this->tx_pin_->digital_write(true);
    delayMicroseconds(300);
  }

  // Wait for a response.
  // The time to first bit varies between installations for unknown reasons.
  // In some installations, 8500 us is sufficient whereas others need as much as 14000 us
  // to respond reliably. Refer to issue #2 for details.
  constexpr uint32_t FIRST_BIT_TIMEOUT = 14000; // Typical range from 7500 us to 13000 us
  constexpr uint32_t NEXT_BIT_TIMEOUT = 200;    // Typically 120 us ON / 50 us OFF
  uint32_t timeout = FIRST_BIT_TIMEOUT;
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
            ESP_LOGW(TAG, "No response from tank %d", tank);
          } else {
            ESP_LOGW(TAG, "Timeout while reading data from tank %d", tank);
          }
          return false;
        }
      }
      start = end;
      timeout = NEXT_BIT_TIMEOUT;
      while (this->rx_pin_->digital_read()) {
        end = micros();
        if (end - start > timeout) {
          ESP_LOGW(TAG, "Timeout while reading data from tank %d", tank);
          return false;
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
    return false;
  }

  const uint8_t* data = packet + 2;
  std::copy(data, data + 10, out_data->begin());
  return true;
}

}  // namespace seelevel
}  // namespace esphome
