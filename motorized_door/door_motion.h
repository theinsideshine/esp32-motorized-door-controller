#ifndef DOOR_MOTION_H
#define DOOR_MOTION_H

#include <Arduino.h>
#include <math.h>
#include "door_config.h"
#include "timer.h"

/*
  ============================================================
  CDoorMotion
  ------------------------------------------------------------
  Responsabilidad:
    - Movimiento/posicionamiento automatico hacia un setpoint.
    - Maquina interna de posicionamiento: idle/start/moving/settling.
    - Estado HOLDING reservado para futuro control cerrado de posicion.
    - Criterios de llegada, cruce, stall, timeout y cancelacion.
    - Summary final del movimiento automatico.

  Regla de arquitectura:
    - NO procesa JSON.
    - NO guarda NVS.
    - NO conoce pines.
    - NO crea clases de motor ni sensor.
    - Usa callbacks a funciones fisicas ya validadas en el main.
    - Mantiene modo fixed PWM como baseline y perfil approach no-PID.
    - Agrega motion_mode=2 con PID de posicion para llegar y cortar.
    - La integral se usa solo en movimiento, con limite y zona activa.
    - HOLDING queda preparado, pero todavia no se activa.
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
  DOOR_MOTION_SETTLING,
  DOOR_MOTION_HOLDING
};

struct DoorMotionCallbacks {
  float (*read_sensor_deg)(bool count_for_motion_stats);
  uint32_t (*get_last_sensor_read_us)();
  bool (*is_fc_l_active)();
  void (*motor_right_continuous)(uint8_t pwm);
  void (*motor_left_continuous)(uint8_t pwm);
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
  bool is_busy() const;
  bool is_moving_or_starting() const;
  bool is_settling() const;
  bool is_holding() const;

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

  CTimer runTimer;
  CTimer stallTimer;
  CTimer debugTimer;
  CTimer settleTimer;

  float lastStallDeg;
  int stallCount;

  const char* finishReason;
  bool finishWasCancel;

  CTimer controlTimer;
  bool sampleTimingStarted;

  uint32_t samples;
  uint32_t minSampleDtUs;
  uint32_t maxSampleDtUs;
  uint32_t maxSensorUs;
  uint32_t maxControlUs;

  uint8_t activePwm;

  float pdLastDeg;
  float pdVelocityDegS;
  float pdTermP;
  float pdTermI;
  float pdTermD;
  float pdCommand;
  float pidIntegralAccum;

  void reset_stats();
  void register_sample_timing(uint32_t dtUs);

  float read_sensor(bool countForMotionStats);

  uint8_t compute_motion_pwm(float absErrorDeg) const;
  uint8_t compute_pid_pwm(float errorDeg, float velocityDegS, float dtSec, DoorMotionDirection& desiredDirection);
  void apply_motion_pwm(uint8_t pwm);
  void apply_pd_output(DoorMotionDirection desiredDirection, uint8_t pwm);

  void start_step();
  void moving_step();
  void complete_settling_if_ready();

  void enter_settling(const char* reason, bool wasCancel, float currentDeg, float errorDeg);
  void finish_now(const char* reason, float currentDeg, float errorDeg);
  void cancel_now(const char* reason, float currentDeg, float errorDeg);

  void print_summary(const char* reason, float finalDeg, float finalErrorDeg);
};

#endif // DOOR_MOTION_H
