/** 
 *  nRF51 Servo Controller
 * 
 * Copyright (C) 2016, Uri Shaked.
 * 
 * License: MIT
 */

#include "softdevice_handler.h"
#include <Ticker.h>
#include <Arduino.h>

#include "nrf51servo.h"

#define SERVO_TIMESLOT_LENGTH  2500 // ms

static mbed::Ticker servo_task;
static uint8_t   servo_count = 0;
static Servo *servos[MAX_SERVOS] = {0};

static nrf_radio_signal_callback_return_param_t signal_callback_return_param = {0};

static nrf_radio_signal_callback_return_param_t * radio_callback(uint8_t signal_type)
{
  uint16_t next_tick = 0;
  uint16_t servo_tick_counter = 0;

  switch (signal_type)
  {
    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_START:
      signal_callback_return_param.params.request.p_next = NULL;
      signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_END;

      servo_tick_counter = servos[0]->readMicroseconds();
      for (uint8_t i = 0; i < servo_count; i++) {
        uint16_t ms = servos[i]->readMicroseconds();
        if (ms < servo_tick_counter) {
          servo_tick_counter = ms;
        }
        digitalWrite(servos[i]->getPin(), 1);
      }

      delayMicroseconds(servo_tick_counter);

      do {
        next_tick = 0;
        for (uint8_t i = 0; i < servo_count; i++) {
          uint16_t ms = servos[i]->readMicroseconds();
          if (ms <= servo_tick_counter) {
            digitalWrite(servos[i]->getPin(), 0);
          } else if ((next_tick == 0) || (ms < next_tick)) {
            next_tick = ms;
          }
        }
        if (next_tick > 0) {
            delayMicroseconds(next_tick - servo_tick_counter);
            servo_tick_counter = next_tick;
        }
      } while (next_tick > 0);
      break;

    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_RADIO:
      signal_callback_return_param.params.request.p_next = NULL;
      signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
      break;

    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_TIMER0:
      signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_END;
      signal_callback_return_param.params.request.p_next = NULL;
      break;

    default:
      signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
      signal_callback_return_param.params.request.p_next = NULL;
      break;
  }
  return (&signal_callback_return_param);
}

static nrf_radio_request_t   timeslot_request = {0};
static void servo_task_callback() {
  // Request a timeslot from the NRF firmware
  timeslot_request.request_type               = NRF_RADIO_REQ_TYPE_EARLIEST;
  timeslot_request.params.earliest.hfclk      = NRF_RADIO_HFCLK_CFG_DEFAULT;
  timeslot_request.params.earliest.priority   = NRF_RADIO_PRIORITY_NORMAL;
  timeslot_request.params.earliest.length_us  = SERVO_TIMESLOT_LENGTH;
  timeslot_request.params.earliest.timeout_us = 100000;
  sd_radio_request(&timeslot_request);
}

static void start_servo_task() {
  sd_radio_session_open(radio_callback);
  servo_task_callback();
  servo_task.attach_us(servo_task_callback, SERVO_REFRESH_INTERVAL);
}

static void stop_servo_task() {
  servo_task.detach();
  sd_radio_session_close();
}

Servo::Servo() {
  this->servoIndex = INVALID_SERVO;
  this->pulseWidth = DEFAULT_PULSE_WIDTH;
}

Servo::~Servo() {
  this->detach();
}

uint8_t Servo::attach(uint32_t pin) {
  bool first = (servo_count == 0);
  this->pin = pin;
  if ((this->servoIndex == INVALID_SERVO) && (servo_count < MAX_SERVOS)) {
    this->servoIndex = servo_count;
    servos[this->servoIndex] = this;
    servo_count++;
    pinMode(pin, OUTPUT);
    if (first) {
      start_servo_task();
    }
  }
  return this->servoIndex;
}

void Servo::detach() {
  if (this->servoIndex != INVALID_SERVO) {
    for (uint8_t i = this->servoIndex + 1; i < servo_count; i++) {
      servos[i] = servos[i - 1];
    }
    servo_count--;
    if (servo_count == 0) {
      stop_servo_task();
    }
  }
  this->servoIndex = INVALID_SERVO;
}

void Servo::write(uint16_t value) {
  value = map(value, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  this->writeMicroseconds(value);
}

void Servo::writeMicroseconds(uint16_t value) {
  if (value < MIN_PULSE_WIDTH) {
    value = MIN_PULSE_WIDTH;
  }
  if (value > MAX_PULSE_WIDTH) {
    value = MAX_PULSE_WIDTH;
  }
  this->pulseWidth = value;
}

uint16_t Servo::read() {
  return map(this->pulseWidth, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH, 0, 180);
}

uint16_t Servo::readMicroseconds() {
  return this->pulseWidth;
}

bool Servo::attached() {
  return this->servoIndex != INVALID_SERVO;
}

uint32_t Servo::getPin() {
  return this->pin;
}

