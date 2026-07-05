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

  activePwm = 0;

  pdLastDeg = 0.0f;
  pdVelocityDegS = 0.0f;
  pdTermP = 0.0f;
  pdTermI = 0.0f;
  pdTermD = 0.0f;
  pdCommand = 0.0f;

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

  if (is_busy()) {
    Serial.println("POSITION FSM ocupada. Solo se acepta stop para cancelar.");
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

    case DOOR_MOTION_HOLDING:
      // Reservado para futura etapa con mantenimiento de posicion.
      // En v4.1b motion_mode=2 llega y corta; todavia no entra en HOLDING.
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
    return;
  }

  if (state == DOOR_MOTION_HOLDING) {
    cb.stop_motor_output_only();
    direction = DOOR_MOTION_DIR_NONE;
    state = DOOR_MOTION_IDLE;
  }
}

bool CDoorMotion::is_active() const
{
  return state != DOOR_MOTION_IDLE;
}

bool CDoorMotion::is_busy() const
{
  return state == DOOR_MOTION_START ||
         state == DOOR_MOTION_MOVING ||
         state == DOOR_MOTION_SETTLING;
}

bool CDoorMotion::is_moving_or_starting() const
{
  return state == DOOR_MOTION_START || state == DOOR_MOTION_MOVING;
}

bool CDoorMotion::is_settling() const
{
  return state == DOOR_MOTION_SETTLING;
}

bool CDoorMotion::is_holding() const
{
  return state == DOOR_MOTION_HOLDING;
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

    case DOOR_MOTION_HOLDING:
      return "HOLDING";

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

uint8_t CDoorMotion::compute_motion_pwm(float absErrorDeg) const
{
  if (cfg == nullptr) {
    return 0;
  }

  if (cfg->get_motion_mode() == DOOR_MOTION_MODE_APPROACH) {
    unsigned long elapsedMs = millis() - startMs;

    if (elapsedMs < cfg->get_start_boost_ms()) {
      return (uint8_t)cfg->get_pwm_start();
    }

    if (cfg->get_slow_zone_deg() > 0.0f && absErrorDeg <= cfg->get_slow_zone_deg()) {
      return (uint8_t)cfg->get_pwm_slow();
    }
  }

  return (uint8_t)cfg->get_pwm_move();
}

uint8_t CDoorMotion::compute_pd_pwm(float errorDeg, float velocityDegS, DoorMotionDirection& desiredDirection)
{
  desiredDirection = DOOR_MOTION_DIR_NONE;

  if (cfg == nullptr) {
    pdTermP = 0.0f;
    pdTermI = 0.0f;
    pdTermD = 0.0f;
    pdCommand = 0.0f;
    return 0;
  }

  // angle_error_deg() usa error = current - target.
  // Para el PD usamos error de control = target - current, por eso se invierte el signo.
  float controlErrorDeg = -errorDeg;

  pdTermP = cfg->get_pid_kp() * controlErrorDeg;

  // v4.1b: Ki queda configurado y persistido, pero no se usa todavia.
  // La integral se habilitara recien cuando el PD este validado y pasemos a HOLDING.
  pdTermI = 0.0f;

  // Derivada sobre velocidad medida/estimada para evitar golpe derivativo por cambio de setpoint.
  pdTermD = -cfg->get_pid_kd() * velocityDegS;

  pdCommand = pdTermP + pdTermI + pdTermD;

  if (pdCommand > 0.0f) {
    desiredDirection = DOOR_MOTION_DIR_LEFT;   // sube angulo
  } else if (pdCommand < 0.0f) {
    desiredDirection = DOOR_MOTION_DIR_RIGHT;  // baja angulo
  } else {
    return 0;
  }

  float pwmFloat = fabs(pdCommand);
  float absErrorDeg = fabs(errorDeg);

  if (pwmFloat > (float)cfg->get_pid_pwm_max()) {
    pwmFloat = (float)cfg->get_pid_pwm_max();
  }

  // Lejos del target se eleva al PWM minimo efectivo para vencer friccion.
  // Cerca del target no se fuerza minimo para no producir sobrepaso innecesario.
  if (absErrorDeg > cfg->get_pid_min_effective_error_deg() &&
      pwmFloat > 0.0f &&
      pwmFloat < (float)cfg->get_pid_pwm_min_effective()) {
    pwmFloat = (float)cfg->get_pid_pwm_min_effective();
  }

  if (pwmFloat < 0.5f) {
    return 0;
  }

  return (uint8_t)(pwmFloat + 0.5f);
}

void CDoorMotion::apply_motion_pwm(uint8_t pwm)
{
  activePwm = pwm;

  if (direction == DOOR_MOTION_DIR_RIGHT) {
    cb.motor_right_continuous(activePwm);
    return;
  }

  if (direction == DOOR_MOTION_DIR_LEFT) {
    cb.motor_left_continuous(activePwm);
  }
}

void CDoorMotion::apply_pd_output(DoorMotionDirection desiredDirection, uint8_t pwm)
{
  direction = desiredDirection;
  activePwm = pwm;

  if (direction == DOOR_MOTION_DIR_NONE || activePwm == 0) {
    cb.stop_motor_output_only();
    return;
  }

  if (direction == DOOR_MOTION_DIR_RIGHT) {
    cb.motor_right_continuous(activePwm);
    return;
  }

  if (direction == DOOR_MOTION_DIR_LEFT) {
    cb.motor_left_continuous(activePwm);
  }
}

void CDoorMotion::print_summary(const char* reason, float finalDeg, float finalErrorDeg)
{
  if (cfg != nullptr && cfg->get_log_level() == DOOR_LOG_LEVEL_PLOTTER) {
    return;
  }

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

  Serial.print("pwm_move=");
  Serial.println(cfg->get_pwm_move());

  Serial.print("motion_mode=");
  Serial.println(cfg->get_motion_mode());

  if (cfg->get_motion_mode() == DOOR_MOTION_MODE_APPROACH) {
    Serial.print("pwm_start=");
    Serial.println(cfg->get_pwm_start());

    Serial.print("pwm_slow=");
    Serial.println(cfg->get_pwm_slow());

    Serial.print("slow_zone_deg=");
    Serial.println(cfg->get_slow_zone_deg(), 2);

    Serial.print("start_boost_ms=");
    Serial.println(cfg->get_start_boost_ms());
  }

  if (cfg->get_motion_mode() == DOOR_MOTION_MODE_PD_POSITION) {
    Serial.print("pid_kp=");
    Serial.println(cfg->get_pid_kp(), 4);

    Serial.print("pid_ki_used=");
    Serial.println(0.0f, 4);

    Serial.print("pid_kd=");
    Serial.println(cfg->get_pid_kd(), 4);

    Serial.print("pid_pwm_max=");
    Serial.println(cfg->get_pid_pwm_max());

    Serial.print("pid_pwm_min_effective=");
    Serial.println(cfg->get_pid_pwm_min_effective());

    Serial.print("pid_min_effective_error_deg=");
    Serial.println(cfg->get_pid_min_effective_error_deg(), 2);

    Serial.print("pd_last_velocity_deg_s=");
    Serial.println(pdVelocityDegS, 2);

    Serial.print("pd_last_u=");
    Serial.println(pdCommand, 2);
  }

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

  if (finishWasCancel && cfg->get_log_level() != DOOR_LOG_LEVEL_PLOTTER) {
    Serial.print("AUTO CANCELADO: ");
    Serial.println(finishReason);
  }

  print_summary(finishReason, finalDeg, finalError);

  direction = DOOR_MOTION_DIR_NONE;

  // v4.1b: incluso en motion_mode=2 el flujo sigue siendo llegar y cortar.
  // HOLDING queda preparado para la proxima etapa, pero todavia no se activa.
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

  activePwm = 0;

  pdLastDeg = startDeg;
  pdVelocityDegS = 0.0f;
  pdTermP = 0.0f;
  pdTermI = 0.0f;
  pdTermD = 0.0f;
  pdCommand = 0.0f;

  reset_stats();

  float absStartErrorDeg = fabs(startErrorDeg);
  uint8_t startPwm = 0;
  DoorMotionDirection startDirection = DOOR_MOTION_DIR_NONE;

  if (cfg->get_motion_mode() == DOOR_MOTION_MODE_PD_POSITION) {
    startPwm = compute_pd_pwm(startErrorDeg, 0.0f, startDirection);
  } else {
    startPwm = compute_motion_pwm(absStartErrorDeg);
  }

  if (cfg->get_log_level() != DOOR_LOG_LEVEL_PLOTTER) {
    Serial.println();

    if (cfg->get_motion_mode() == DOOR_MOTION_MODE_PD_POSITION) {
      Serial.print("AUTO START PD POSITION -> ");
    } else {
      Serial.print("AUTO START CONTINUO SILENCIOSO -> ");
    }

    Serial.print(targetName);
    Serial.print("  current=");
    Serial.print(startDeg, 2);
    Serial.print("  target=");
    Serial.print(targetDeg, 2);
    Serial.print("  error=");
    Serial.print(startErrorDeg, 2);
    Serial.print("  pwm=");
    Serial.print(startPwm);
    Serial.print("  motion_mode=");
    Serial.println(cfg->get_motion_mode());
  }

  if (fabs(startErrorDeg) <= cfg->get_auto_tolerance_deg()) {
    direction = DOOR_MOTION_DIR_NONE;
    state = DOOR_MOTION_IDLE;

    print_summary("ya_en_posicion", startDeg, startErrorDeg);
    return;
  }

  if (cfg->get_motion_mode() == DOOR_MOTION_MODE_PD_POSITION) {
    if (cfg->get_log_level() != DOOR_LOG_LEVEL_PLOTTER) {
      if (startDirection == DOOR_MOTION_DIR_RIGHT) {
        Serial.println("AUTO DIR: RIGHT / REWIND logico / baja angulo");
      } else if (startDirection == DOOR_MOTION_DIR_LEFT) {
        Serial.println("AUTO DIR: LEFT / FORWARD logico / sube angulo");
      } else {
        Serial.println("AUTO DIR: NONE / PD sin salida inicial");
      }
    }

    apply_pd_output(startDirection, startPwm);
  } else if (startErrorDeg > 0.0f) {
    // current > target: bajar angulo
    direction = DOOR_MOTION_DIR_RIGHT;

    if (cfg->get_log_level() != DOOR_LOG_LEVEL_PLOTTER) {
      Serial.println("AUTO DIR: RIGHT / REWIND logico / baja angulo");
    }

    apply_motion_pwm(startPwm);
  } else {
    // current < target: subir angulo
    direction = DOOR_MOTION_DIR_LEFT;

    if (cfg->get_log_level() != DOOR_LOG_LEVEL_PLOTTER) {
      Serial.println("AUTO DIR: LEFT / FORWARD logico / sube angulo");
    }

    apply_motion_pwm(startPwm);
  }

  // Forzamos que el primer control se ejecute enseguida.
  lastControlUs = micros() - cfg->get_control_period_us();
  state = DOOR_MOTION_MOVING;
}

void CDoorMotion::moving_step()
{
  uint32_t nowUs = micros();
  uint32_t dtUs = nowUs - lastControlUs;

  if (dtUs < cfg->get_control_period_us()) {
    return;
  }

  lastControlUs = nowUs;

  uint32_t controlStartUs = micros();

  register_sample_timing(nowUs);

  float dtSec = (float)dtUs / 1000000.0f;

  if (dtSec <= 0.0f || dtSec > 0.5f) {
    dtSec = (float)cfg->get_control_period_us() / 1000000.0f;
  }

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

  if (cfg->get_motion_mode() == DOOR_MOTION_MODE_PD_POSITION) {
    float deltaDeg = angle_error_deg(currentDeg, pdLastDeg);
    pdVelocityDegS = deltaDeg / dtSec;
    pdLastDeg = currentDeg;

    DoorMotionDirection desiredDirection = DOOR_MOTION_DIR_NONE;
    uint8_t desiredPwm = compute_pd_pwm(errorDeg, pdVelocityDegS, desiredDirection);

    if (desiredDirection != direction || desiredPwm != activePwm) {
      apply_pd_output(desiredDirection, desiredPwm);
    }
  } else {
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

    uint8_t desiredPwm = compute_motion_pwm(absErrorDeg);

    if (desiredPwm != activePwm) {
      apply_motion_pwm(desiredPwm);
    }
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
  if (debugAuto &&
      cfg->get_log_level() != DOOR_LOG_LEVEL_PLOTTER &&
      millis() - lastDebugMs >= DOOR_MOTION_DEBUG_PERIOD_MS) {
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
    Serial.print(activePwm);
    Serial.print(" mode=");
    Serial.print(cfg->get_motion_mode());

    if (cfg->get_motion_mode() == DOOR_MOTION_MODE_PD_POSITION) {
      Serial.print(" vel_deg_s=");
      Serial.print(pdVelocityDegS, 2);
      Serial.print(" u=");
      Serial.print(pdCommand, 2);
    }

    Serial.println();
  }

  uint32_t controlUs = micros() - controlStartUs;

  if (controlUs > maxControlUs) {
    maxControlUs = controlUs;
  }
}
