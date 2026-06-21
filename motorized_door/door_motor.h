#ifndef DOOR_MOTOR_H
#define DOOR_MOTOR_H

#include <Arduino.h>

/*
  ============================================================
  CDoorMotor
  ------------------------------------------------------------
  Responsabilidad:
    - Encapsular la salida fisica al DRV8833.
    - Mantener STBY, AIN1, AIN2, PWM y estado de motor.
    - No decide posicion, no lee sensor, no procesa JSON.

  Regla de arquitectura:
    - El modulo de movimiento decide LEFT/RIGHT/STOP.
    - Esta clase solo ejecuta la salida fisica validada.
    - El PWM se recibe como parametro para no acoplar motor a Config.
  ============================================================
*/

enum DoorMotorState : uint8_t {
  DOOR_MOTOR_STOPPED = 0,
  DOOR_MOTOR_RIGHT,
  DOOR_MOTOR_LEFT
};

class CDoorMotor {
public:
  CDoorMotor();

  void begin(uint8_t stbyPin,
             uint8_t ain1Pin,
             uint8_t ain2Pin,
             uint32_t pwmFreq,
             uint8_t pwmRes);

  void coast();
  void stop_brake();

  void enable();
  void disable();

  void stop_only();
  void stop_output_only();

  void right_continuous(uint8_t pwm);
  void left_continuous(uint8_t pwm);

  DoorMotorState state() const;
  const char* state_name() const;
  bool is_enabled() const;
  bool is_stopped() const;

private:
  uint8_t pinStby;
  uint8_t pinAin1;
  uint8_t pinAin2;

  uint32_t freq;
  uint8_t res;

  bool initialized;
  bool enabled;
  DoorMotorState motorState;
};

#endif // DOOR_MOTOR_H
