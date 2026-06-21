#ifndef DOOR_ANGLE_SENSOR_H
#define DOOR_ANGLE_SENSOR_H

#include <Arduino.h>
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/as5048a/MagneticSensorAS5048A.h"

/*
  CDoorAngleSensor - Step 6a conservador

  Esta clase encapsula la lectura/medicion del AS5048A, pero NO
  inicializa SPI ni construye el objeto MagneticSensorAS5048A.

  Motivo: Step 6 original movio tambien la inicializacion/objeto del
  sensor y en hardware la lectura quedo clavada en 0.00.
  En Step 6a se mantiene la inicializacion AS5048A exactamente como
  Step 5 validado, y solo se mueve la logica de lectura/estado.
*/

class CDoorAngleSensor {
public:
  explicit CDoorAngleSensor(MagneticSensorAS5048A& sensorRef);

  float read_deg();

  float deg() const;
  float rad() const;
  float vel_rad_s() const;
  uint16_t raw() const;
  uint32_t last_read_us() const;

private:
  static float normalize360(float deg);

  MagneticSensorAS5048A* sensor;

  float lastDeg;
  float lastRad;
  float lastVelRadS;
  uint16_t lastRaw;
  uint32_t lastSensorReadUs;
};

#endif // DOOR_ANGLE_SENSOR_H
