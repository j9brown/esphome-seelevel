#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include <array>
#include <cstdint>

namespace esphome {
namespace seelevel {

class SeelevelComponent : public Component {
 public:
  enum class ReadResult {
    OK = 0,
    FAILED = 1,
    RATE_LIMITED = 2,
  };

  void set_rx_pin(GPIOPin* pin) { this->rx_pin_ = pin; }
  void set_tx_pin(GPIOPin* pin) { this->tx_pin_ = pin; }

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;

  using SegmentData = std::array<uint8_t, 10>;
  ReadResult read_tank(unsigned tank, SegmentData* out_data);

 protected:
  bool read_tank_with_tx_active_(unsigned tank, SegmentData* out_data);

  GPIOPin* rx_pin_{nullptr};
  GPIOPin* tx_pin_{nullptr};
  uint32_t last_read_time_{0x80000000};
};

}  // namespace emc2101
}  // namespace esphome
