#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace seelevel {

class SeelevelComponent : public Component {
 public:
  void set_rx_pin(GPIOPin* pin) { this->rx_pin_ = pin; }
  void set_tx_pin(GPIOPin* pin) { this->tx_pin_ = pin; }

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;

  float read_tank(unsigned tank, unsigned segments);

 protected:
  float read_tank_with_tx_active_(unsigned tank, unsigned segments);

  GPIOPin* rx_pin_{nullptr};
  GPIOPin* tx_pin_{nullptr};
};

}  // namespace emc2101
}  // namespace esphome
