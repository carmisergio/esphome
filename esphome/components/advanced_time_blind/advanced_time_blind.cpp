#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "advanced_time_blind.h"
#include <ctime>

namespace esphome {
namespace advanced_time_blind {

// Constants
constexpr float COVER_MIDPOINT = 0.5f;
constexpr uint32_t MOVING_UPDATE_INTERVAL = 500;  // (ms)

static const char *TAG = "advanced_time_blind.cover";

void AdvancedTimeBlind::setup() {
  // Initialize pins
  this->pin_up_->pin_mode(gpio::Flags::FLAG_OUTPUT);
  this->pin_down_->pin_mode(gpio::Flags::FLAG_OUTPUT);
  this->pin_up_->digital_write(false);
  this->pin_down_->digital_write(false);

  // Initialize state
  this->current_operation = cover::CoverOperation::COVER_OPERATION_IDLE;
  this->state_ = State::IDLE;

  // Restore position
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->set_new_target_(cover::COVER_CLOSED);
  }
}

void AdvancedTimeBlind::loop() {
  uint32_t time = millis();

  switch (this->state_) {
    case State::MOVING_UP:
    case State::MOVING_DOWN:
      // Compute current position
      this->position = this->compute_cur_pos_(time);

      if (this->state_ == State::MOVING_UP ? this->position >= this->target_pos_
                                           : this->position <= this->target_pos_) {
        // Blind has reached target
        this->position = this->target_pos_;
        this->current_operation = cover::CoverOperation::COVER_OPERATION_IDLE;

        // Do we need to add extra time for the endstop calibration?
        if ((this->position == cover::COVER_CLOSED || this->position == cover::COVER_OPEN) &&
            this->endstop_extra_time_ != 0) {
          this->state_ = State::AT_ENDSTOP;
          this->start_time_ = time;
        } else {
          this->stop_movement_();
          this->state_ = State::IDLE;
        }

        this->publish_state();
      } else {
        // If time since the last state update has exceeded the interval, publish state update
        if (time - this->last_update_time_ > MOVING_UPDATE_INTERVAL) {
          this->publish_state();
          this->last_update_time_ = time;
        }
      }
      break;
    case State::AT_ENDSTOP:
      if (time - this->start_time_ >= this->endstop_extra_time_) {
        this->stop_movement_();
        this->state_ = State::IDLE;
      }
      break;
    default:
      break;
  }
}

void AdvancedTimeBlind::dump_config() { ESP_LOGCONFIG(TAG, "Empty cover"); }

cover::CoverTraits AdvancedTimeBlind::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_is_assumed_state(false);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);

  return traits;
}

void AdvancedTimeBlind::control(const cover::CoverCall &call) {
  // Stop
  if (call.get_stop()) {
    this->stop_();
  }

  // Toggle
  if (call.get_toggle().has_value()) {
    // Action table
    //  - Moving -> Stopped
    //  - Open (>= 50%) -> Closing
    //  - Closed (< 50%) -> Opening
    if (this->state_ == State::IDLE) {
      // What is the opposite of the current position?
      if (this->position >= COVER_MIDPOINT) {
        this->set_new_target_(cover::COVER_CLOSED);
      } else {
        this->set_new_target_(cover::COVER_OPEN);
      }
    } else {
      this->stop_();
    }
  }

  // Set position
  if (call.get_position().has_value()) {
    float pos = *call.get_position();
    this->set_new_target_(pos);
  }

  // Publish state
  this->publish_state();
}

void AdvancedTimeBlind::stop_() {
  // Stop movement
  this->stop_movement_();

  // Update state
  this->state_ = State::IDLE;
  this->current_operation = cover::CoverOperation::COVER_OPERATION_IDLE;
  this->position = this->compute_cur_pos_(millis());
}

void AdvancedTimeBlind::set_new_target_(float target) {
  //// Get current position
  // float cur_pos = this->compute_cur_pos_();

  // If we magically are already at the right position, stop
  if (this->position == target) {
    this->stop_();
    return;
  }

  // Figure out new direction of motion
  State tmp_state = target >= this->position ? State::MOVING_UP : State::MOVING_DOWN;

  // Only change state if the new state is not aligned with the current one
  if (tmp_state != this->state_) {
    if (tmp_state == State::MOVING_UP) {
      this->move_up_();
      this->current_operation = cover::CoverOperation::COVER_OPERATION_OPENING;
      this->state_ = State::MOVING_UP;
    } else {
      this->move_down_();
      this->current_operation = cover::CoverOperation::COVER_OPERATION_CLOSING;
      this->state_ = State::MOVING_DOWN;
    }

    // Save start time of this movement
    uint32_t time = millis();
    this->start_time_ = time;
    this->last_update_time_ = time;
    this->start_pos_ = this->position;

    this->state_ = tmp_state;
  }

  // Update target position
  this->target_pos_ = target;
}

float AdvancedTimeBlind::compute_cur_pos_(uint32_t time) {
  // If not moving, the actual position is coherent with the published state
  if (this->state_ == State::IDLE) {
    return this->position;
  } else {
    uint32_t elapsed_time = time - this->start_time_;

    // Get correct timingsd based on operation
    uint32_t start_offset = this->state_ == State::MOVING_UP ? this->start_offset_up_ : this->start_offset_down_;
    uint32_t full_time = this->state_ == State::MOVING_UP ? this->time_up_ : this->time_down_;

    // Subtract start offset from elapsed time
    if (elapsed_time < start_offset) {
      return this->position;
    } else {
      elapsed_time -= start_offset;
    }

    // Compute position delta based on time
    float pos_delta = (float) elapsed_time / full_time;

    // Apply position delta
    return this->state_ == State::MOVING_UP ? this->start_pos_ + pos_delta : this->start_pos_ - pos_delta;
  }
}

void AdvancedTimeBlind::stop_movement_() {
  this->pin_up_->digital_write(false);
  this->pin_down_->digital_write(false);
}

void AdvancedTimeBlind::move_up_() {
  this->pin_down_->digital_write(false);
  this->pin_up_->digital_write(true);
}

void AdvancedTimeBlind::move_down_() {
  this->pin_up_->digital_write(false);
  this->pin_down_->digital_write(true);
}

}  // namespace advanced_time_blind
}  // namespace esphome
