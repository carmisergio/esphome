#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace advanced_time_blind {

class AdvancedTimeBlind : public cover::Cover, public Component {
  // Movement state
  enum State : uint8_t {
    IDLE,
    MOVING_UP,
    MOVING_DOWN,
    AT_ENDSTOP,
  };

 public:
  // Configuration setters
  void set_pin_up(GPIOPin *pin_up) { pin_up_ = pin_up; }
  void set_pin_down(GPIOPin *pin_down) { pin_down_ = pin_down; }
  void set_time_up(uint32_t time_up) { time_up_ = time_up; };
  void set_time_down(uint32_t time_down) { time_down_ = time_down; };
  void set_start_offset_up(uint32_t start_offset_up) { start_offset_up_ = start_offset_up; };
  void set_start_offset_down(uint32_t start_offset_down) { start_offset_down_ = start_offset_down; };
  void set_endstop_extra_time(uint32_t endstop_extra_time) { endstop_extra_time_ = endstop_extra_time; };

  // Lifetime functions
  void setup() override;
  void loop() override;
  void dump_config() override;
  cover::CoverTraits get_traits() override;

 protected:
  // Component class methods
  void control(const cover::CoverCall &call) override;

 private:
  // Start moving towards a new target position
  void set_new_target_(float target);

  // Stop motion towards target
  void stop_();

  // Compute current position during movment
  float compute_cur_pos_(uint32_t time);

  // Movmenet control
  void stop_movement_();
  void move_up_();
  void move_down_();

  // Configuration
  GPIOPin *pin_up_, *pin_down_;
  uint32_t time_up_, start_offset_up_;
  uint32_t time_down_, start_offset_down_;
  uint32_t endstop_extra_time_;

  // State
  State state_;
  float target_pos_, start_pos_;
  uint32_t start_time_, last_update_time_;
};

}  // namespace advanced_time_blind
}  // namespace esphome
