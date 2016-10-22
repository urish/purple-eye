/** 
 *  nRF51 Servo Controller
 * 
 * Copyright (C) 2016, Uri Shaked.
 * 
 * License: MIT
 */

#ifndef _NRF_SERVO_H
#define _NRF_SERVO_H

#include <inttypes.h>

#define MAX_SERVOS             10

#define INVALID_SERVO          255     // Value representing invalid servo index

// Min and max pulse length, in microseconds
#define MIN_PULSE_WIDTH        544     
#define MAX_PULSE_WIDTH        2400

#define SERVO_REFRESH_INTERVAL 10000   // Refresh servos every 10ms
#define DEFAULT_PULSE_WIDTH    1500     // default pulse width when servo is attached

class Servo {
  private:
    uint8_t servoIndex;
    uint16_t pulseWidth;
    uint32_t pin;
    
  public:
    Servo();
    ~Servo();
    
    uint8_t attach(uint32_t pin);
    void detach();
    
    void write(uint16_t value);
    void writeMicroseconds(uint16_t value);
    
    uint16_t read();
    uint16_t readMicroseconds();
    
    bool attached();    // return true if this servo is attached, otherwise false

    uint32_t getPin();
};

#endif /* _NRF_SERVO_H */

