#ifndef DOOR_MOTION_H
#define DOOR_MOTION_H

#include <Arduino.h>
#include <math.h>
#include "door_config.h"

/*
  ============================================================
  CDoorMotion
  ------------------------------------------------------------
  Responsabilidad:
    - Movimiento/posicionamiento automatico hacia un setpoint.
    - Maquina interna de posicionamiento: idle/start/moving/settling.
    - Criterios de llegada, cruce, stall, timeout y cancelacion.
    - Summary final del movimiento automatico.

  Regla de arquitectura:
    - NO procesa JSON.
    - NO guarda NVS.
    - NO conoce pines.
    - NO crea clases de motor ni sensor.
    - Usa callbacks a funciones fisicas ya validadas en el main.
    - Mantiene PWM fijo; PID todavia NO entra en esta version.
  ============================================================
*/

#define DOOR_MOTION_DEBUG_PERIOD_MS 250UL

enum DoorMotionDirection : uint8_t {
  DOOR_MOTION_DIR_NONE = 0,
  DOOR_MOTION_DIR_RIGHT,
  DOOR_MOTION_DIR_LEFT
};

enum DoorMotionState : uint8_t {
  DOOR_MOTION_IDLE = 0,
  DOOR_MOTION_START,
  DOOR_MOTION_MOVING,
  DOOR_MOTION_SETTLING
};

struct DoorMotionCallbacks {
  float (*read_sensor_deg)(bool count_for_motion_stats);
  uint32_t (*get_last_sensor_read_us)();
  bool (*is_fc_l_active)();
  void (*motor_right_continuous)();
  void (*motor_left_continuous)();
  void (*stop_motor_output_only)();
};

class CDoorMotion {
public:
  CDoorMotion();

  void begin(CDoorConfig* config, const DoorMotionCallbacks& callbacks);

  bool start(float targetDeg, const char* targetName);
  void update();
  void cancel(const char* reason);

  bool is_active() const;
  bool is_moving_or_starting() const;
  bool is_settling() const;

  void set_debug(bool enabled);
  bool get_debug() const;

  const char* state_name() const;
  const char* direction_name() const;

  static float normalize360(float deg);
  static float angle_error_deg(float currentDeg, float targetDeg);
  static float angle_distance_deg(float aDeg, float bDeg);

private:
  CDoorConfig* cfg;
  DoorMotionCallbacks cb;
  bool ready;
  bool debugAuto;

  DoorMotionDirection direction;
  DoorMotionState state;

  float targetDeg;
  const char* targetName;

  float pendingTargetDeg;
  const char* pendingTargetName;

  float startDeg;
  float startErrorDeg;

  float decisionDeg;
  float decisionErrorDeg;

  unsigned long startMs;
  unsigned long lastStallCheckMs;
  unsigned long lastDebugMs;
  unsigned long settleStartMs;

  float lastStallDeg;
  int stallCount;

  const char* finishReason;
  bool finishWasCancel;

  uint32_t lastControlUs;
  uint32_t lastSampleUs;

  uint32_t samples;
  uint32_t minSampleDtUs;
  uint32_t maxSampleDtUs;
  uint32_t maxSensorUs;
  uint32_t maxControlUs;

  void reset_stats();
  void register_sample_timing(uint32_t nowUs);

  float read_sensor(bool countForMotionStats);

  void start_step();
  void moving_step();
  void complete_settling_if_ready();

  void enter_settling(const char* reason, bool wasCancel, float currentDeg, float errorDeg);
  void finish_now(const char* reason, float currentDeg, float errorDeg);
  void cancel_now(const char* reason, float currentDeg, float errorDeg);

  void print_summary(const char* reason, float finalDeg, float finalErrorDeg);
};

#endif // DOOR_MOTION_H
