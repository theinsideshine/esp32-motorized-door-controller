// C:\Users\ptavolaro\AppData\Local\arduino\sketches

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/as5048a/MagneticSensorAS5048A.h"
#include "door_config.h"

/*
  ============================================================
  PROYECTO: ESP32 MOTORIZED DOOR CONTROLLER
  VERSION: v3.1-continuous-silent-measured-config-json-position-fsm-step3

  OBJETIVO DE ESTA VERSION
  ------------------------------------------------------------
  Version de movimiento continuo hacia 3 posiciones, pero con
  Serial silencioso durante el movimiento automatico para no
  afectar el tiempo de muestreo/control.

  Esta version busca medir:
    - si llega o no llega
    - cuanto se pasa
    - tiempo total de movimiento
    - cantidad de muestras tomadas
    - periodo real de muestreo
    - tiempo maximo de lectura del sensor
    - tiempo maximo de calculo/control

  Concepto a validar:
    Con PWM fijo, la respuesta depende de la mecanica real,
    carga, rozamiento, tension, inercia y sentido de movimiento.
    Esta version sirve para buscar PWM minimo y PWM maximo util.

  Hardware:
    - ESP32-S3 Dev Module
    - DRV8833
    - Motor N20/reductor a 6 V
    - AS5048A por SPI
    - FC_L en GPIO14 con INPUT_PULLUP, NC a GND

  Pines AS5048A:
    CSn  -> GPIO10
    MOSI -> GPIO11
    SCK  -> GPIO12
    MISO -> GPIO13
    VDD5V -> 5V0
    GND -> GND comun

  Pines DRV8833:
    STBY -> GPIO4
    AIN2 -> GPIO16
    AIN1 -> GPIO17
    VM -> 6 V motor
    GND comun con ESP32

  Sentido validado:
    - MOTOR LEFT  / FORWARD logico -> sube angulo
    - MOTOR RIGHT / REWIND logico  -> baja angulo

  IMPORTANTE:
    Durante POSITION_MOVING no se imprime printSensor().
    Solo imprime:
      - comando recibido
      - inicio de automatico
      - resumen final
      - anomalias

  PASO DE ABSTRACCION 2:
    Se mantiene CDoorConfig validado en step2.

    En este paso se formaliza la maquina de estados de posicionamiento:
      - POSITION_IDLE
      - POSITION_START
      - POSITION_MOVING
      - POSITION_SETTLING

    Se diferencia conceptualmente el estado general del dispositivo
    del estado interno del posicionamiento.

    El control fisico validado se mantiene igual:
      - mismos comandos JSON
      - mismos sentidos
      - mismos criterios de llegada/cruce/stall/timeout
      - mismo summary final
  ============================================================
*/

// ============================================================
// VERSION
// ============================================================

#define APP_VERSION "v3.1-continuous-silent-measured-config-json-position-fsm-step3"

// ============================================================
// PINES
// ============================================================

// DRV8833
#define STBY_PIN 4
#define AIN2_PIN 16
#define AIN1_PIN 17

// Final de carrera izquierdo
#define FC_L_PIN 14

// AS5048A SPI
#define AS5048_CS   10
#define AS5048_MOSI 11
#define AS5048_SCK  12
#define AS5048_MISO 13

// ============================================================
// CONFIGURACION GENERAL
// ============================================================

#define SERIAL_BAUD 115200

#define PWM_FREQ 1000
#define PWM_RES  8

#define PWM_TEST 70
#define PWM_STEP 5
#define PWM_MIN  0
#define PWM_MAX  255

#define TIMEOUT_MANUAL_MS 35

#define POS_MEDIO_APROX_DEG 315.0f

// Posiciones calibradas reales.
// Se cargan al iniciar el ESP32.
// Los comandos a/b/c pueden cambiarlas en RAM,
// pero no se guardan en memoria permanente.
#define POS_1_DEFAULT_DEG 2.29f
#define POS_2_DEFAULT_DEG 291.23f
#define POS_3_DEFAULT_DEG 206.06f

// Control automatico continuo
#define AUTO_TOLERANCE_DEG       1.50f
#define AUTO_CROSS_MARGIN_DEG    0.20f
#define AUTO_MAX_RUN_MS          10000UL
#define AUTO_STALL_CHECK_MS      250UL
#define AUTO_MIN_MOVE_DEG        0.30f
#define AUTO_STALL_MAX_COUNT     3

// Periodo de control automatico.
// 5000 us = 5 ms = 200 muestras/seg maximo teorico.
#define CONTROL_PERIOD_US        5000UL

// Espera despues de frenar antes de leer posicion final estable.
#define AUTO_FINAL_SETTLE_MS     80UL

// Debug automatico opcional.
// Por defecto apagado.
#define AUTO_DEBUG_PERIOD_MS     250UL

// Stream manual de posicion.
// Por defecto apagado para no cargar Serial.
#define STREAM_PERIOD_MS         100UL

// ============================================================
// SENSOR
// ============================================================

MagneticSensorAS5048A sensor(AS5048_CS);

// Configuracion persistente + comunicacion host.
CDoorConfig Config;

// ============================================================
// ESTADOS
// ============================================================

/*
  Hay dos niveles conceptuales:

  1) DeviceState
     Estado general del producto/dispositivo.
     En este paso se usa solo lo necesario para no cambiar comportamiento.

  2) PositionState
     Estado interno del movimiento automatico hacia una posicion.
     Esta es la maquina que antes estaba implicita en el flag autoMoveActive.

  MotorState no decide nada: solo refleja la salida fisica aplicada
  al DRV8833.
*/

enum MotorState {
  MOTOR_STOPPED,
  MOTOR_RIGHT,
  MOTOR_LEFT
};

enum AutoDirection {
  AUTO_DIR_NONE,
  AUTO_DIR_RIGHT,
  AUTO_DIR_LEFT
};

enum DeviceState {
  DEV_IDLE,
  DEV_MANUAL_MOVING,
  DEV_POSITIONING
};

enum PositionState {
  POSITION_IDLE,
  POSITION_START,
  POSITION_MOVING,
  POSITION_SETTLING
};

MotorState motorState = MOTOR_STOPPED;
AutoDirection autoDirection = AUTO_DIR_NONE;
DeviceState deviceState = DEV_IDLE;
PositionState positionState = POSITION_IDLE;

bool driverEnabled = false;
bool streamEnabled = false;
bool debugAuto = false;


// Ultima lectura del sensor
float lastDeg = 0.0f;
float lastRad = 0.0f;
float lastVelRadS = 0.0f;
uint16_t lastRaw = 0;

// Manual
unsigned long lastManualMoveMs = 0;

// Stream
unsigned long lastStreamMs = 0;

// Automatico / posicionamiento
float autoTargetDeg = POS_MEDIO_APROX_DEG;
const char* autoTargetName = "NINGUNA";

float pendingTargetDeg = POS_MEDIO_APROX_DEG;
const char* pendingTargetName = "NINGUNA";

float autoStartDeg = 0.0f;
float autoStartErrorDeg = 0.0f;

float autoDecisionDeg = 0.0f;
float autoDecisionErrorDeg = 0.0f;

unsigned long autoStartMs = 0;
unsigned long autoLastStallCheckMs = 0;
unsigned long autoLastDebugMs = 0;
unsigned long autoSettleStartMs = 0;

float autoLastStallDeg = 0.0f;
int autoStallCount = 0;

const char* autoFinishReason = "ninguna";
bool autoFinishWasCancel = false;

// Medicion de tiempos
uint32_t autoLastControlUs = 0;
uint32_t autoLastSampleUs = 0;

uint32_t autoSamples = 0;
uint32_t autoMinSampleDtUs = 0xFFFFFFFF;
uint32_t autoMaxSampleDtUs = 0;
uint32_t autoMaxSensorUs = 0;
uint32_t autoMaxControlUs = 0;

bool isPositionActive() {
  return positionState != POSITION_IDLE;
}

// ============================================================
// UTILIDADES ANGULARES
// ============================================================

float normalize360(float deg) {
  while (deg < 0.0f) {
    deg += 360.0f;
  }

  while (deg >= 360.0f) {
    deg -= 360.0f;
  }

  return deg;
}

// error = current - target normalizado a [-180, 180]
// Si error > 0: current esta por arriba del target en el camino corto.
// Si error < 0: current esta por debajo del target en el camino corto.
float angleErrorDeg(float currentDeg, float targetDeg) {
  float error = currentDeg - targetDeg;

  while (error > 180.0f) {
    error -= 360.0f;
  }

  while (error < -180.0f) {
    error += 360.0f;
  }

  return error;
}

float angleDistanceDeg(float aDeg, float bDeg) {
  return fabs(angleErrorDeg(aDeg, bDeg));
}

const char* motorStateName() {
  switch (motorState) {
    case MOTOR_RIGHT:
      return "RIGHT";
    case MOTOR_LEFT:
      return "LEFT";
    default:
      return "STOP";
  }
}

const char* autoDirectionName() {
  switch (autoDirection) {
    case AUTO_DIR_RIGHT:
      return "RIGHT";
    case AUTO_DIR_LEFT:
      return "LEFT";
    default:
      return "NONE";
  }
}

const char* deviceStateName() {
  switch (deviceState) {
    case DEV_MANUAL_MOVING:
      return "MANUAL_MOVING";
    case DEV_POSITIONING:
      return "POSITIONING";
    default:
      return "IDLE";
  }
}

const char* positionStateName() {
  switch (positionState) {
    case POSITION_START:
      return "START";
    case POSITION_MOVING:
      return "MOVING";
    case POSITION_SETTLING:
      return "SETTLING";
    default:
      return "IDLE";
  }
}

// ============================================================
// SENSOR
// ============================================================

float readSensorDegMeasured(bool countForAutoStats) {
  uint32_t t0 = micros();

  sensor.update();

  uint32_t sensorUs = micros() - t0;

  lastRad = sensor.getAngle();
  lastDeg = normalize360(lastRad * 57.2957795f);
  lastVelRadS = sensor.getVelocity();
  lastRaw = 0;

  if (countForAutoStats && positionState == POSITION_MOVING) {
    if (sensorUs > autoMaxSensorUs) {
      autoMaxSensorUs = sensorUs;
    }
  }

  return lastDeg;
}

bool isFcLActive() {
  return digitalRead(FC_L_PIN) == HIGH;
}

void printSensor() {
  Serial.print("raw=");
  Serial.print(lastRaw);

  Serial.print("  deg=");
  Serial.print(lastDeg, 2);

  Serial.print("  mid=");
  Serial.print(POS_MEDIO_APROX_DEG, 2);

  Serial.print("  delta_mid=");
  Serial.print(angleErrorDeg(lastDeg, POS_MEDIO_APROX_DEG), 2);

  Serial.print("  rad=");
  Serial.print(lastRad, 5);

  Serial.print("  vel_rad_s=");
  Serial.print(lastVelRadS, 5);

  Serial.print("  motor=");
  Serial.print(motorStateName());

  Serial.print("  pwm=");
  Serial.print(Config.get_pwm_move());

  Serial.print("  auto=");
  Serial.print(isPositionActive() ? "ON" : "OFF");

  Serial.print("  device=");
  Serial.print(deviceStateName());

  Serial.print("  position=");
  Serial.print(positionStateName());

  Serial.print("  FC_L=");
  Serial.println(isFcLActive() ? "ACTIVO" : "NORMAL");
}

// ============================================================
// MOTOR / DRV8833
// ============================================================

void motorCoast() {
  ledcWrite(AIN1_PIN, 0);
  ledcWrite(AIN2_PIN, 0);
}

void motorStopBrake() {
  ledcWrite(AIN1_PIN, 255);
  ledcWrite(AIN2_PIN, 255);
  delay(30);
  motorCoast();
}

void driverEnable() {
  if (driverEnabled) {
    return;
  }

  digitalWrite(STBY_PIN, HIGH);
  delay(10);
  driverEnabled = true;
}

void driverDisable() {
  motorCoast();
  digitalWrite(STBY_PIN, LOW);
  driverEnabled = false;
}

void stopMotorOnly() {
  motorStopBrake();
  motorState = MOTOR_STOPPED;
  deviceState = DEV_IDLE;
  driverDisable();
}

void motorRightContinuous() {
  driverEnable();

  // RIGHT / REWIND logico: baja angulo.
  // Mantener este mapeo porque fue validado en v3.0.
  ledcWrite(AIN1_PIN, Config.get_pwm_move());
  ledcWrite(AIN2_PIN, 0);

  motorState = MOTOR_RIGHT;
}

void motorLeftContinuous() {
  driverEnable();

  // LEFT / FORWARD logico: sube angulo.
  // Mantener este mapeo porque fue validado en v3.0.
  ledcWrite(AIN1_PIN, 0);
  ledcWrite(AIN2_PIN, Config.get_pwm_move());

  motorState = MOTOR_LEFT;
}

void commandRightManual() {
  Serial.println("CMD MOTOR RIGHT MANUAL");
  deviceState = DEV_MANUAL_MOVING;
  motorRightContinuous();
  lastManualMoveMs = millis();
}

void commandLeftManual() {
  Serial.println("CMD MOTOR LEFT MANUAL");
  deviceState = DEV_MANUAL_MOVING;
  motorLeftContinuous();
  lastManualMoveMs = millis();
}

void checkMotorTimeout() {
  if (isPositionActive()) {
    return;
  }

  if (motorState == MOTOR_STOPPED) {
    return;
  }

  if (millis() - lastManualMoveMs >= TIMEOUT_MANUAL_MS) {
    Serial.println("AUTO STOP por timeout manual");
    stopMotorOnly();

    readSensorDegMeasured(false);
    printSensor();
  }
}

// ============================================================
// MEDICION AUTO
// ============================================================

void resetAutoStats() {
  autoLastControlUs = micros();
  autoLastSampleUs = 0;

  autoSamples = 0;
  autoMinSampleDtUs = 0xFFFFFFFF;
  autoMaxSampleDtUs = 0;
  autoMaxSensorUs = 0;
  autoMaxControlUs = 0;
}

void registerAutoSampleTiming(uint32_t nowUs) {
  if (autoLastSampleUs != 0) {
    uint32_t dt = nowUs - autoLastSampleUs;

    if (dt < autoMinSampleDtUs) {
      autoMinSampleDtUs = dt;
    }

    if (dt > autoMaxSampleDtUs) {
      autoMaxSampleDtUs = dt;
    }
  }

  autoLastSampleUs = nowUs;
  autoSamples++;
}

void printAutoSummary(const char* reason, float finalDeg, float finalErrorDeg) {
  unsigned long runMs = millis() - autoStartMs;

  Serial.println();
  Serial.println("========== AUTO SUMMARY ==========");

  Serial.print("target_name=");
  Serial.println(autoTargetName);

  Serial.print("reason=");
  Serial.println(reason);

  Serial.print("start_deg=");
  Serial.println(autoStartDeg, 2);

  Serial.print("target_deg=");
  Serial.println(autoTargetDeg, 2);

  Serial.print("start_error_deg=");
  Serial.println(autoStartErrorDeg, 2);

  Serial.print("decision_deg=");
  Serial.println(autoDecisionDeg, 2);

  Serial.print("decision_error_deg=");
  Serial.println(autoDecisionErrorDeg, 2);

  Serial.print("final_deg=");
  Serial.println(finalDeg, 2);

  Serial.print("final_error_deg=");
  Serial.println(finalErrorDeg, 2);

  Serial.print("pwm=");
  Serial.println(Config.get_pwm_move());

  Serial.print("direction=");
  Serial.println(autoDirectionName());

  Serial.print("run_ms=");
  Serial.println(runMs);

  Serial.print("samples=");
  Serial.println(autoSamples);

  Serial.print("control_period_us=");
  Serial.println(Config.get_control_period_us());

  Serial.print("min_sample_dt_us=");
  if (autoSamples <= 1 || autoMinSampleDtUs == 0xFFFFFFFF) {
    Serial.println(0);
  } else {
    Serial.println(autoMinSampleDtUs);
  }

  Serial.print("max_sample_dt_us=");
  Serial.println(autoMaxSampleDtUs);

  Serial.print("max_sensor_us=");
  Serial.println(autoMaxSensorUs);

  Serial.print("max_control_us=");
  Serial.println(autoMaxControlUs);

  Serial.print("stall_count=");
  Serial.print(autoStallCount);
  Serial.print("/");
  Serial.println(Config.get_auto_stall_max_count());

  Serial.println("==================================");
  Serial.println();
}

// ============================================================
// CONTROL AUTOMATICO CONTINUO / POSITION FSM
// ============================================================

void stopMotorOutputOnly() {
  motorStopBrake();
  motorState = MOTOR_STOPPED;
  driverDisable();
}

void positionEnterSettling(const char* reason, bool wasCancel, float decisionDeg, float decisionErrorDeg) {
  autoDecisionDeg = decisionDeg;
  autoDecisionErrorDeg = decisionErrorDeg;

  autoFinishReason = reason;
  autoFinishWasCancel = wasCancel;

  stopMotorOutputOnly();

  autoSettleStartMs = millis();
  positionState = POSITION_SETTLING;
}

void positionFinishNow(const char* reason, float decisionDeg, float decisionErrorDeg) {
  positionEnterSettling(reason, false, decisionDeg, decisionErrorDeg);
}

void positionCancelNow(const char* reason, float decisionDeg, float decisionErrorDeg) {
  positionEnterSettling(reason, true, decisionDeg, decisionErrorDeg);
}

void positionCompleteSettlingIfReady() {
  if (positionState != POSITION_SETTLING) {
    return;
  }

  if (millis() - autoSettleStartMs < Config.get_auto_final_settle_ms()) {
    return;
  }

  float finalDeg = readSensorDegMeasured(false);
  float finalError = angleErrorDeg(finalDeg, autoTargetDeg);

  if (autoFinishWasCancel) {
    Serial.print("AUTO CANCELADO: ");
    Serial.println(autoFinishReason);
  }

  printAutoSummary(autoFinishReason, finalDeg, finalError);

  autoDirection = AUTO_DIR_NONE;
  positionState = POSITION_IDLE;
  deviceState = DEV_IDLE;
}

void startPositionMove(float targetDeg, const char* targetName) {
  if (isPositionActive()) {
    Serial.println("POSITION FSM activa. Solo se acepta stop para cancelar.");
    return;
  }

  pendingTargetDeg = normalize360(targetDeg);
  pendingTargetName = targetName;

  deviceState = DEV_POSITIONING;
  positionState = POSITION_START;
}

void positionStartStep() {
  readSensorDegMeasured(false);

  autoTargetDeg = pendingTargetDeg;
  autoTargetName = pendingTargetName;

  autoStartDeg = lastDeg;
  autoStartErrorDeg = angleErrorDeg(autoStartDeg, autoTargetDeg);

  autoDecisionDeg = autoStartDeg;
  autoDecisionErrorDeg = autoStartErrorDeg;

  autoStartMs = millis();
  autoLastStallCheckMs = millis();
  autoLastDebugMs = millis();

  autoLastStallDeg = autoStartDeg;
  autoStallCount = 0;

  autoFinishReason = "ninguna";
  autoFinishWasCancel = false;

  resetAutoStats();

  Serial.println();
  Serial.print("AUTO START CONTINUO SILENCIOSO -> ");
  Serial.print(autoTargetName);
  Serial.print("  current=");
  Serial.print(autoStartDeg, 2);
  Serial.print("  target=");
  Serial.print(autoTargetDeg, 2);
  Serial.print("  error=");
  Serial.print(autoStartErrorDeg, 2);
  Serial.print("  pwm=");
  Serial.println(Config.get_pwm_move());

  if (fabs(autoStartErrorDeg) <= Config.get_auto_tolerance_deg()) {
    autoDirection = AUTO_DIR_NONE;
    positionState = POSITION_IDLE;
    deviceState = DEV_IDLE;

    printAutoSummary("ya_en_posicion", autoStartDeg, autoStartErrorDeg);
    return;
  }

  if (autoStartErrorDeg > 0.0f) {
    // current > target: bajar angulo
    autoDirection = AUTO_DIR_RIGHT;
    Serial.println("AUTO DIR: RIGHT / REWIND logico / baja angulo");
    motorRightContinuous();
  } else {
    // current < target: subir angulo
    autoDirection = AUTO_DIR_LEFT;
    Serial.println("AUTO DIR: LEFT / FORWARD logico / sube angulo");
    motorLeftContinuous();
  }

  // Forzamos que el primer control se ejecute enseguida.
  autoLastControlUs = micros() - Config.get_control_period_us();
  positionState = POSITION_MOVING;
}

void positionMovingStep() {
  uint32_t nowUs = micros();

  if ((uint32_t)(nowUs - autoLastControlUs) < Config.get_control_period_us()) {
    return;
  }

  autoLastControlUs = nowUs;

  uint32_t controlStartUs = micros();

  registerAutoSampleTiming(nowUs);

  float currentDeg = readSensorDegMeasured(true);
  float errorDeg = angleErrorDeg(currentDeg, autoTargetDeg);
  float absErrorDeg = fabs(errorDeg);

  autoDecisionDeg = currentDeg;
  autoDecisionErrorDeg = errorDeg;

  if (isFcLActive()) {
    positionCancelNow("FC_L_ACTIVO", currentDeg, errorDeg);
    return;
  }

  if (millis() - autoStartMs >= Config.get_auto_max_run_ms()) {
    positionCancelNow("tiempo_maximo_alcanzado", currentDeg, errorDeg);
    return;
  }

  // Caso ideal: entro dentro de tolerancia.
  if (absErrorDeg <= Config.get_auto_tolerance_deg()) {
    positionFinishNow("posicion_alcanzada", currentDeg, errorDeg);
    return;
  }

  // Si cruce el objetivo, corto.
  // RIGHT baja angulo: el error deberia pasar de positivo a negativo.
  if (autoDirection == AUTO_DIR_RIGHT && errorDeg < -Config.get_auto_cross_margin_deg()) {
    positionFinishNow("objetivo_cruzado", currentDeg, errorDeg);
    return;
  }

  // LEFT sube angulo: el error deberia pasar de negativo a positivo.
  if (autoDirection == AUTO_DIR_LEFT && errorDeg > Config.get_auto_cross_margin_deg()) {
    positionFinishNow("objetivo_cruzado", currentDeg, errorDeg);
    return;
  }

  // Deteccion de sin movimiento.
  if (millis() - autoLastStallCheckMs >= Config.get_auto_stall_check_ms()) {
    float movedDeg = angleDistanceDeg(currentDeg, autoLastStallDeg);

    if (movedDeg < Config.get_auto_min_move_deg()) {
      autoStallCount++;
    } else {
      autoStallCount = 0;
    }

    autoLastStallDeg = currentDeg;
    autoLastStallCheckMs = millis();

    if (autoStallCount >= Config.get_auto_stall_max_count()) {
      positionCancelNow("sin_movimiento_detectado", currentDeg, errorDeg);
      return;
    }
  }

  // Debug compacto opcional. Por defecto apagado.
  if (debugAuto && millis() - autoLastDebugMs >= AUTO_DEBUG_PERIOD_MS) {
    autoLastDebugMs = millis();

    Serial.print("AUTO ");
    Serial.print(autoTargetName);
    Serial.print(" current=");
    Serial.print(currentDeg, 2);
    Serial.print(" target=");
    Serial.print(autoTargetDeg, 2);
    Serial.print(" error=");
    Serial.print(errorDeg, 2);
    Serial.print(" dir=");
    Serial.print(autoDirectionName());
    Serial.print(" samples=");
    Serial.print(autoSamples);
    Serial.print(" pwm=");
    Serial.println(Config.get_pwm_move());
  }

  uint32_t controlUs = micros() - controlStartUs;

  if (controlUs > autoMaxControlUs) {
    autoMaxControlUs = controlUs;
  }
}

void updatePositionFsm() {
  switch (positionState) {
    case POSITION_START:
      positionStartStep();
      break;

    case POSITION_MOVING:
      positionMovingStep();
      break;

    case POSITION_SETTLING:
      positionCompleteSettlingIfReady();
      break;

    case POSITION_IDLE:
    default:
      break;
  }
}

// ============================================================
// COMANDOS SERIAL
// ============================================================

void printVersion() {
  Serial.println();
  Serial.print("APP_VERSION=");
  Serial.println(APP_VERSION);

  Serial.print("PWM actual=");
  Serial.println(Config.get_pwm_move());

  Serial.print("deviceState=");
  Serial.println(deviceStateName());

  Serial.print("positionState=");
  Serial.println(positionStateName());

  Serial.print("CONTROL_PERIOD_US=");
  Serial.println(Config.get_control_period_us());

  Serial.print("AUTO_TOLERANCE_DEG=");
  Serial.println(Config.get_auto_tolerance_deg(), 2);

  Serial.print("AUTO_MAX_RUN_MS=");
  Serial.println(Config.get_auto_max_run_ms());

  Serial.print("AUTO_STALL_CHECK_MS=");
  Serial.println(Config.get_auto_stall_check_ms());

  Serial.print("AUTO_MIN_MOVE_DEG=");
  Serial.println(Config.get_auto_min_move_deg(), 2);

  Serial.print("AUTO_STALL_MAX_COUNT=");
  Serial.println(Config.get_auto_stall_max_count());

  Serial.print("stream=");
  Serial.println(streamEnabled ? "ON" : "OFF");

  Serial.print("debugAuto=");
  Serial.println(debugAuto ? "ON" : "OFF");

  Serial.println();
}

void printPositions() {
  Serial.println();
  Serial.println("POSICIONES ACTUALES EN RAM:");

  Serial.print("POS_1=");
  Serial.print(Config.get_pos1_deg(), 2);
  Serial.print(" deg  hardcoded=");
  Serial.println(POS_1_DEFAULT_DEG, 2);

  Serial.print("POS_2=");
  Serial.print(Config.get_pos2_deg(), 2);
  Serial.print(" deg  hardcoded=");
  Serial.println(POS_2_DEFAULT_DEG, 2);

  Serial.print("POS_3=");
  Serial.print(Config.get_pos3_deg(), 2);
  Serial.print(" deg  hardcoded=");
  Serial.println(POS_3_DEFAULT_DEG, 2);

  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println("============================================================");
  Serial.println(APP_VERSION);
  Serial.println("HOST JSON ONLY:");
  Serial.println("  {\"info\":\"all-params\"}");
  Serial.println("  {\"cmd\":\"go\",\"pos\":1}");
  Serial.println("  {\"cmd\":\"go\",\"pos\":2}");
  Serial.println("  {\"cmd\":\"go\",\"pos\":3}");
  Serial.println("  {\"cmd\":\"stop\"}");
  Serial.println("  {\"pwm_move\":70}");
  Serial.println("  {\"pos1_deg\":2.29}");
  Serial.println("  {\"pos2_deg\":291.23}");
  Serial.println("  {\"pos3_deg\":206.06}");
  Serial.println("  {\"cmd\":\"factory-reset\"}");
  Serial.println("============================================================");
  Serial.println();
}

void processHostRequest() {
  if (!Config.has_request()) {
    return;
  }

  DoorHostRequest req = Config.get_request();
  Config.clear_request();

  if (isPositionActive() && req != DOOR_REQ_STOP) {
    Serial.println("AUTO activo: solo se acepta stop para cancelar.");
    return;
  }

  switch (req) {
    case DOOR_REQ_STOP:
      if (positionState == POSITION_MOVING || positionState == POSITION_START) {
        readSensorDegMeasured(false);
        positionCancelNow("cancelado_por_host", lastDeg, angleErrorDeg(lastDeg, autoTargetDeg));
      } else if (positionState == POSITION_SETTLING) {
        // Ya se corto el motor y se esta esperando la lectura final estable.
        // No se cambia el reason original.
      } else {
        stopMotorOnly();
      }
      break;

    case DOOR_REQ_GO_POS_1:
      startPositionMove(Config.get_pos_deg(1), "POS_1");
      break;

    case DOOR_REQ_GO_POS_2:
      startPositionMove(Config.get_pos_deg(2), "POS_2");
      break;

    case DOOR_REQ_GO_POS_3:
      startPositionMove(Config.get_pos_deg(3), "POS_3");
      break;

    default:
      break;
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(20);
  delay(1000);

  if (!Config.init()) {
    Serial.println("ERROR: no se pudo inicializar Config/NVS. Se usan defaults RAM.");
  }

  pinMode(STBY_PIN, OUTPUT);
  pinMode(FC_L_PIN, INPUT_PULLUP);

  digitalWrite(STBY_PIN, LOW);
  driverEnabled = false;
  deviceState = DEV_IDLE;

  ledcAttach(AIN1_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(AIN2_PIN, PWM_FREQ, PWM_RES);

  motorCoast();

  SPI.begin(AS5048_SCK, AS5048_MISO, AS5048_MOSI, AS5048_CS);
  sensor.init(&SPI);

  Serial.println();
  Serial.print("BOOT OK - ");
  Serial.println(APP_VERSION);
  Serial.println("Host: JSON only. Ejemplo: {\"info\":\"all-params\"}");

  delay(200);

  readSensorDegMeasured(false);
  printSensor();

  Serial.println("Sistema listo. Stream apagado por defecto.");
}

void loop() {
  Config.host_cmd();
  processHostRequest();

  if (isPositionActive()) {
    updatePositionFsm();
    return;
  }

  checkMotorTimeout();

  if (streamEnabled && millis() - lastStreamMs >= STREAM_PERIOD_MS) {
    lastStreamMs = millis();

    readSensorDegMeasured(false);
    printSensor();
  }
}
