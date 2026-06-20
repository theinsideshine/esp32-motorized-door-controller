#include "door_motion.h"

CDoorMotion::CDoorMotion()
{
  cfg = nullptr;
  ready = false;
  debugAuto = false;

  direction = DOOR_MOTION_DIR_NONE;
  state = DOOR_MOTION_IDLE;

  targetDeg = 315.0f;
  targetName = "NINGUNA";

  pendingTargetDeg = 315.0f;
  pendingTargetName = "NINGUNA";

  startDeg = 0.0f;
  startErrorDeg = 0.0f;

  decisionDeg = 0.0f;
  decisionErrorDeg = 0.0f;

  startMs = 0;
  lastStallCheckMs = 0;
  lastDebugMs = 0;
  settleStartMs = 0;

  lastStallDeg = 0.0f;
  stallCount = 0;

  finishReason = "ninguna";
  finishWasCancel = false;

  lastControlUs = 0;
  lastSampleUs = 0;

  samples = 0;
  minSampleDtUs = 0xFFFFFFFF;
  maxSampleDtUs = 0;
  maxSensorUs = 0;
  maxControlUs = 0;

  cb.read_sensor_deg = nullptr;
  cb.get_last_sensor_read_us = nullptr;
  cb.is_fc_l_active = nullptr;
  cb.motor_right_continuous = nullptr;
  cb.motor_left_continuous = nullptr;
  cb.stop_motor_output_only = nullptr;
}

void CDoorMotion::begin(CDoorConfig* config, const DoorMotionCallbacks& callbacks)
{
  cfg = config;
  cb = callbacks;

  ready = (cfg != nullptr) &&
          (cb.read_sensor_deg != nullptr) &&
          (cb.get_last_sensor_read_us != nullptr) &&
          (cb.is_fc_l_active != nullptr) &&
          (cb.motor_right_continuous != nullptr) &&
          (cb.motor_left_continuous != nullptr) &&
          (cb.stop_motor_output_only != nullptr);

  direction = DOOR_MOTION_DIR_NONE;
  state = DOOR_MOTION_IDLE;
}

bool CDoorMotion::start(float requestedTargetDeg, const char* requestedTargetName)
{
  if (!ready) {
    Serial.println("ERROR: DoorMotion no inicializado.");
    return false;
  }

  if (is_active()) {
    Serial.println("POSITION FSM activa. Solo se acepta stop para cancelar.");
    return false;
  }

  pendingTargetDeg = normalize360(requestedTargetDeg);
  pendingTargetName = requestedTargetName;

  state = DOOR_MOTION_START;
  return true;
}

void CDoorMotion::update()
{
  if (!ready) {
    return;
  }

  switch (state) {
    case DOOR_MOTION_START:
      start_step();
      break;

    case DOOR_MOTION_MOVING:
      moving_step();
      break;

    case DOOR_MOTION_SETTLING:
      complete_settling_if_ready();
      break;

    case DOOR_MOTION_IDLE:
    default:
      break;
  }
}

void CDoorMotion::cancel(const char* reason)
{
  if (!ready) {
    return;
  }

  if (state == DOOR_MOTION_START) {
    targetDeg = pendingTargetDeg;
    targetName = pendingTargetName;
  }

  if (state == DOOR_MOTION_START || state == DOOR_MOTION_MOVING) {
    float currentDeg = read_sensor(false);
    float errorDeg = angle_error_deg(currentDeg, targetDeg);
    cancel_now(reason, currentDeg, errorDeg);
  }
}

bool CDoorMotion::is_active() const
{
  return state != DOOR_MOTION_IDLE;
}

bool CDoorMotion::is_moving_or_starting() const
{
  return state == DOOR_MOTION_START || state == DOOR_MOTION_MOVING;
}

bool CDoorMotion::is_settling() const
{
  return state == DOOR_MOTION_SETTLING;
}

void CDoorMotion::set_debug(bool enabled)
{
  debugAuto = enabled;
}

bool CDoorMotion::get_debug() const
{
  return debugAuto;
}

const char* CDoorMotion::state_name() const
{
  switch (state) {
    case DOOR_MOTION_START:
      return "START";

    case DOOR_MOTION_MOVING:
      return "MOVING";

    case DOOR_MOTION_SETTLING:
      return "SETTLING";

    case DOOR_MOTION_IDLE:
    default:
      return "IDLE";
  }
}

const char* CDoorMotion::direction_name() const
{
  switch (direction) {
    case DOOR_MOTION_DIR_RIGHT:
      return "RIGHT";

    case DOOR_MOTION_DIR_LEFT:
      return "LEFT";

    case DOOR_MOTION_DIR_NONE:
    default:
      return "NONE";
  }
}

float CDoorMotion::normalize360(float deg)
{
  while (deg < 0.0f) {
    deg += 360.0f;
  }

  while (deg >= 360.0f) {
    deg -= 360.0f;
  }

  return deg;
}

float CDoorMotion::angle_error_deg(float currentDeg, float targetDeg)
{
  float error = currentDeg - targetDeg;

  while (error > 180.0f) {
    error -= 360.0f;
  }

  while (error < -180.0f) {
    error += 360.0f;
  }

  return error;
}

float CDoorMotion::angle_distance_deg(float aDeg, float bDeg)
{
  return fabs(angle_error_deg(aDeg, bDeg));
}

void CDoorMotion::reset_stats()
{
  lastControlUs = micros();
  lastSampleUs = 0;

  samples = 0;
  minSampleDtUs = 0xFFFFFFFF;
  maxSampleDtUs = 0;
  maxSensorUs = 0;
  maxControlUs = 0;
}

void CDoorMotion::register_sample_timing(uint32_t nowUs)
{
  if (lastSampleUs != 0) {
    uint32_t dt = nowUs - lastSampleUs;

    if (dt < minSampleDtUs) {
      minSampleDtUs = dt;
    }

    if (dt > maxSampleDtUs) {
      maxSampleDtUs = dt;
    }
  }

  lastSampleUs = nowUs;
  samples++;
}

float CDoorMotion::read_sensor(bool countForMotionStats)
{
  float deg = cb.read_sensor_deg(countForMotionStats);

  if (countForMotionStats && state == DOOR_MOTION_MOVING) {
    uint32_t sensorUs = cb.get_last_sensor_read_us();

    if (sensorUs > maxSensorUs) {
      maxSensorUs = sensorUs;
    }
  }

  return deg;
}

void CDoorMotion::print_summary(const char* reason, float finalDeg, float finalErrorDeg)
{
  unsigned long runMs = millis() - startMs;

  Serial.println();
  Serial.println("========== AUTO SUMMARY ==========");

  Serial.print("target_name=");
  Serial.println(targetName);

  Serial.print("reason=");
  Serial.println(reason);

  Serial.print("start_deg=");
  Serial.println(startDeg, 2);

  Serial.print("target_deg=");
  Serial.println(targetDeg, 2);

  Serial.print("start_error_deg=");
  Serial.println(startErrorDeg, 2);

  Serial.print("decision_deg=");
  Serial.println(decisionDeg, 2);

  Serial.print("decision_error_deg=");
  Serial.println(decisionErrorDeg, 2);

  Serial.print("final_deg=");
  Serial.println(finalDeg, 2);

  Serial.print("final_error_deg=");
  Serial.println(finalErrorDeg, 2);

  Serial.print("pwm=");
  Serial.println(cfg->get_pwm_move());

  Serial.print("direction=");
  Serial.println(direction_name());

  Serial.print("run_ms=");
  Serial.println(runMs);

  Serial.print("samples=");
  Serial.println(samples);

  Serial.print("control_period_us=");
  Serial.println(cfg->get_control_period_us());

  Serial.print("min_sample_dt_us=");
  if (samples <= 1 || minSampleDtUs == 0xFFFFFFFF) {
    Serial.println(0);
  } else {
    Serial.println(minSampleDtUs);
  }

  Serial.print("max_sample_dt_us=");
  Serial.println(maxSampleDtUs);

  Serial.print("max_sensor_us=");
  Serial.println(maxSensorUs);

  Serial.print("max_control_us=");
  Serial.println(maxControlUs);

  Serial.print("stall_count=");
  Serial.print(stallCount);
  Serial.print("/");
  Serial.println(cfg->get_auto_stall_max_count());

  Serial.println("==================================");
  Serial.println();
}

void CDoorMotion::enter_settling(const char* reason, bool wasCancel, float currentDeg, float errorDeg)
{
  decisionDeg = currentDeg;
  decisionErrorDeg = errorDeg;

  finishReason = reason;
  finishWasCancel = wasCancel;

  cb.stop_motor_output_only();

  settleStartMs = millis();
  state = DOOR_MOTION_SETTLING;
}

void CDoorMotion::finish_now(const char* reason, float currentDeg, float errorDeg)
{
  enter_settling(reason, false, currentDeg, errorDeg);
}

void CDoorMotion::cancel_now(const char* reason, float currentDeg, float errorDeg)
{
  enter_settling(reason, true, currentDeg, errorDeg);
}

void CDoorMotion::complete_settling_if_ready()
{
  if (state != DOOR_MOTION_SETTLING) {
    return;
  }

  if (millis() - settleStartMs < cfg->get_auto_final_settle_ms()) {
    return;
  }

  float finalDeg = read_sensor(false);
  float finalError = angle_error_deg(finalDeg, targetDeg);

  if (finishWasCancel) {
    Serial.print("AUTO CANCELADO: ");
    Serial.println(finishReason);
  }

  print_summary(finishReason, finalDeg, finalError);

  direction = DOOR_MOTION_DIR_NONE;
  state = DOOR_MOTION_IDLE;
}

void CDoorMotion::start_step()
{
  float currentDeg = read_sensor(false);

  targetDeg = pendingTargetDeg;
  targetName = pendingTargetName;

  startDeg = currentDeg;
  startErrorDeg = angle_error_deg(startDeg, targetDeg);

  decisionDeg = startDeg;
  decisionErrorDeg = startErrorDeg;

  startMs = millis();
  lastStallCheckMs = millis();
  lastDebugMs = millis();

  lastStallDeg = startDeg;
  stallCount = 0;

  finishReason = "ninguna";
  finishWasCancel = false;

  reset_stats();

  Serial.println();
  Serial.print("AUTO START CONTINUO SILENCIOSO -> ");
  Serial.print(targetName);
  Serial.print("  current=");
  Serial.print(startDeg, 2);
  Serial.print("  target=");
  Serial.print(targetDeg, 2);
  Serial.print("  error=");
  Serial.print(startErrorDeg, 2);
  Serial.print("  pwm=");
  Serial.println(cfg->get_pwm_move());

  if (fabs(startErrorDeg) <= cfg->get_auto_tolerance_deg()) {
    direction = DOOR_MOTION_DIR_NONE;
    state = DOOR_MOTION_IDLE;

    print_summary("ya_en_posicion", startDeg, startErrorDeg);
    return;
  }

  if (startErrorDeg > 0.0f) {
    // current > target: bajar angulo
    direction = DOOR_MOTION_DIR_RIGHT;
    Serial.println("AUTO DIR: RIGHT / REWIND logico / baja angulo");
    cb.motor_right_continuous();
  } else {
    // current < target: subir angulo
    direction = DOOR_MOTION_DIR_LEFT;
    Serial.println("AUTO DIR: LEFT / FORWARD logico / sube angulo");
    cb.motor_left_continuous();
  }

  // Forzamos que el primer control se ejecute enseguida.
  lastControlUs = micros() - cfg->get_control_period_us();
  state = DOOR_MOTION_MOVING;
}

void CDoorMotion::moving_step()
{
  uint32_t nowUs = micros();

  if ((uint32_t)(nowUs - lastControlUs) < cfg->get_control_period_us()) {
    return;
  }

  lastControlUs = nowUs;

  uint32_t controlStartUs = micros();

  register_sample_timing(nowUs);

  float currentDeg = read_sensor(true);
  float errorDeg = angle_error_deg(currentDeg, targetDeg);
  float absErrorDeg = fabs(errorDeg);

  decisionDeg = currentDeg;
  decisionErrorDeg = errorDeg;

  if (cb.is_fc_l_active()) {
    cancel_now("FC_L_ACTIVO", currentDeg, errorDeg);
    return;
  }

  if (millis() - startMs >= cfg->get_auto_max_run_ms()) {
    cancel_now("tiempo_maximo_alcanzado", currentDeg, errorDeg);
    return;
  }

  // Caso ideal: entro dentro de tolerancia.
  if (absErrorDeg <= cfg->get_auto_tolerance_deg()) {
    finish_now("posicion_alcanzada", currentDeg, errorDeg);
    return;
  }

  // Si cruce el objetivo, corto.
  // RIGHT baja angulo: el error deberia pasar de positivo a negativo.
  if (direction == DOOR_MOTION_DIR_RIGHT && errorDeg < -cfg->get_auto_cross_margin_deg()) {
    finish_now("objetivo_cruzado", currentDeg, errorDeg);
    return;
  }

  // LEFT sube angulo: el error deberia pasar de negativo a positivo.
  if (direction == DOOR_MOTION_DIR_LEFT && errorDeg > cfg->get_auto_cross_margin_deg()) {
    finish_now("objetivo_cruzado", currentDeg, errorDeg);
    return;
  }

  // Deteccion de sin movimiento.
  if (millis() - lastStallCheckMs >= cfg->get_auto_stall_check_ms()) {
    float movedDeg = angle_distance_deg(currentDeg, lastStallDeg);

    if (movedDeg < cfg->get_auto_min_move_deg()) {
      stallCount++;
    } else {
      stallCount = 0;
    }

    lastStallDeg = currentDeg;
    lastStallCheckMs = millis();

    if (stallCount >= cfg->get_auto_stall_max_count()) {
      cancel_now("sin_movimiento_detectado", currentDeg, errorDeg);
      return;
    }
  }

  // Debug compacto opcional. Por defecto apagado.
  if (debugAuto && millis() - lastDebugMs >= DOOR_MOTION_DEBUG_PERIOD_MS) {
    lastDebugMs = millis();

    Serial.print("AUTO ");
    Serial.print(targetName);
    Serial.print(" current=");
    Serial.print(currentDeg, 2);
    Serial.print(" target=");
    Serial.print(targetDeg, 2);
    Serial.print(" error=");
    Serial.print(errorDeg, 2);
    Serial.print(" dir=");
    Serial.print(direction_name());
    Serial.print(" samples=");
    Serial.print(samples);
    Serial.print(" pwm=");
    Serial.println(cfg->get_pwm_move());
  }

  uint32_t controlUs = micros() - controlStartUs;

  if (controlUs > maxControlUs) {
    maxControlUs = controlUs;
  }
}
