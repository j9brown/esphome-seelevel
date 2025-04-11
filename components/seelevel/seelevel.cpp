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

bool SeelevelComponent::read_tank(unsigned tank, SegmentData* out_data) {
  if (!this->rx_pin_->digital_read()) {
    ESP_LOGW(TAG, "The sensor interface is malfunctioning: RX is low while TX is low");
    return false;
  }

  // Need to wait a little between consecutive reads otherwise the sensor fails to respond.
  constexpr uint32_t MIN_PAUSE_BETWEEN_READS = 20;
  uint32_t pause_between_reads = millis() - this->last_read_time_;
  if (pause_between_reads < MIN_PAUSE_BETWEEN_READS) {
    delay(MIN_PAUSE_BETWEEN_READS - pause_between_reads);
  }

  this->tx_pin_->digital_write(true);
  delay(3); // charge the sensor, exact timing not important

  bool result = this->read_tank_with_tx_active_(tank, out_data);

  this->tx_pin_->digital_write(false);
  this->last_read_time_ = millis();
  return result;
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
          return false;
        }
      }
      start = end;
      timeout = 200; // shorter timeout for subsequent bits
      while (this->rx_pin_->digital_read()) {
        end = micros();
        if (end - start > timeout) {
          ESP_LOGW(TAG, "Timeout while reading data from tank %d after %d microseconds", tank, end - start);
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
