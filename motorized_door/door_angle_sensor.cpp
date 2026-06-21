#include "door_angle_sensor.h"

CDoorAngleSensor::CDoorAngleSensor(MagneticSensorAS5048A& sensorRef)
  : sensor(&sensorRef)
{
  lastDeg = 0.0f;
  lastRad = 0.0f;
  lastVelRadS = 0.0f;
  lastRaw = 0;
  lastSensorReadUs = 0;
}

float CDoorAngleSensor::normalize360(float deg)
{
  while (deg < 0.0f) {
    deg += 360.0f;
  }

  while (deg >= 360.0f) {
    deg -= 360.0f;
  }

  return deg;
}

float CDoorAngleSensor::read_deg()
{
  uint32_t t0 = micros();

  sensor->update();

  lastSensorReadUs = micros() - t0;

  lastRad = sensor->getAngle();
  lastDeg = normalize360(lastRad * 57.2957795f);
  lastVelRadS = sensor->getVelocity();

  // En esta instalacion de la libreria no se usa getRawCount().
  // Se mantiene raw=0 para conservar el formato de salida validado.
  lastRaw = 0;

  return lastDeg;
}

float CDoorAngleSensor::deg() const
{
  return lastDeg;
}

float CDoorAngleSensor::rad() const
{
  return lastRad;
}

float CDoorAngleSensor::vel_rad_s() const
{
  return lastVelRadS;
}

uint16_t CDoorAngleSensor::raw() const
{
  return lastRaw;
}

uint32_t CDoorAngleSensor::last_read_us() const
{
  return lastSensorReadUs;
}
