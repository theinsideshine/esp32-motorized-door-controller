#include "door_motor.h"

CDoorMotor::CDoorMotor()
{
  pinStby = 0;
  pinAin1 = 0;
  pinAin2 = 0;

  freq = 1000;
  res = 8;

  initialized = false;
  enabled = false;
  motorState = DOOR_MOTOR_STOPPED;
}

void CDoorMotor::begin(uint8_t stbyPin,
                       uint8_t ain1Pin,
                       uint8_t ain2Pin,
                       uint32_t pwmFreq,
                       uint8_t pwmRes)
{
  pinStby = stbyPin;
  pinAin1 = ain1Pin;
  pinAin2 = ain2Pin;
  freq = pwmFreq;
  res = pwmRes;

  pinMode(pinStby, OUTPUT);
  digitalWrite(pinStby, LOW);

  ledcAttach(pinAin1, freq, res);
  ledcAttach(pinAin2, freq, res);

  enabled = false;
  motorState = DOOR_MOTOR_STOPPED;
  initialized = true;

  coast();
}

void CDoorMotor::coast()
{
  if (!initialized) {
    return;
  }

  ledcWrite(pinAin1, 0);
  ledcWrite(pinAin2, 0);
}

void CDoorMotor::stop_brake()
{
  if (!initialized) {
    return;
  }

  ledcWrite(pinAin1, 255);
  ledcWrite(pinAin2, 255);
  delay(30);
  coast();
}

void CDoorMotor::enable()
{
  if (!initialized) {
    return;
  }

  if (enabled) {
    return;
  }

  digitalWrite(pinStby, HIGH);
  delay(10);
  enabled = true;
}

void CDoorMotor::disable()
{
  if (!initialized) {
    return;
  }

  coast();
  digitalWrite(pinStby, LOW);
  enabled = false;
}

void CDoorMotor::stop_only()
{
  stop_brake();
  motorState = DOOR_MOTOR_STOPPED;
  disable();
}

void CDoorMotor::stop_output_only()
{
  stop_brake();
  motorState = DOOR_MOTOR_STOPPED;
  disable();
}

void CDoorMotor::right_continuous(uint8_t pwm)
{
  enable();

  // RIGHT / REWIND logico: baja angulo.
  // Mantener este mapeo porque fue validado en v3.0.
  ledcWrite(pinAin1, pwm);
  ledcWrite(pinAin2, 0);

  motorState = DOOR_MOTOR_RIGHT;
}

void CDoorMotor::left_continuous(uint8_t pwm)
{
  enable();

  // LEFT / FORWARD logico: sube angulo.
  // Mantener este mapeo porque fue validado en v3.0.
  ledcWrite(pinAin1, 0);
  ledcWrite(pinAin2, pwm);

  motorState = DOOR_MOTOR_LEFT;
}

DoorMotorState CDoorMotor::state() const
{
  return motorState;
}

const char* CDoorMotor::state_name() const
{
  switch (motorState) {
    case DOOR_MOTOR_RIGHT:
      return "RIGHT";

    case DOOR_MOTOR_LEFT:
      return "LEFT";

    case DOOR_MOTOR_STOPPED:
    default:
      return "STOP";
  }
}

bool CDoorMotor::is_enabled() const
{
  return enabled;
}

bool CDoorMotor::is_stopped() const
{
  return motorState == DOOR_MOTOR_STOPPED;
}
