// C:\Users\ptavolaro\AppData\Local\arduino\sketches

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/as5048a/MagneticSensorAS5048A.h"

/*
  ============================================================
  PROYECTO: ESP32 MOTORIZED DOOR CONTROLLER
  VERSION: v3.1-continuous-silent-measured

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
    Durante autoMoveActive no se imprime printSensor().
    Solo imprime:
      - comando recibido
      - inicio de automatico
      - resumen final
      - anomalias
  ============================================================
*/

// ============================================================
// VERSION
// ============================================================

#define APP_VERSION "v3.1-continuous-silent-measured"

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

// ============================================================
// ESTADOS
// ============================================================

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

MotorState motorState = MOTOR_STOPPED;
AutoDirection autoDirection = AUTO_DIR_NONE;

bool driverEnabled = false;
bool streamEnabled = false;
bool debugAuto = false;

int pwmManual = PWM_TEST;

float pos1Deg = POS_1_DEFAULT_DEG;
float pos2Deg = POS_2_DEFAULT_DEG;
float pos3Deg = POS_3_DEFAULT_DEG;

// Ultima lectura del sensor
float lastDeg = 0.0f;
float lastRad = 0.0f;
float lastVelRadS = 0.0f;
uint16_t lastRaw = 0;

// Manual
unsigned long lastManualMoveMs = 0;

// Stream
unsigned long lastStreamMs = 0;

// Automatico
bool autoMoveActive = false;

float autoTargetDeg = POS_MEDIO_APROX_DEG;
const char* autoTargetName = "NINGUNA";

float autoStartDeg = 0.0f;
float autoStartErrorDeg = 0.0f;

float autoDecisionDeg = 0.0f;
float autoDecisionErrorDeg = 0.0f;

unsigned long autoStartMs = 0;
unsigned long autoLastStallCheckMs = 0;
unsigned long autoLastDebugMs = 0;

float autoLastStallDeg = 0.0f;
int autoStallCount = 0;

// Medicion de tiempos
uint32_t autoLastControlUs = 0;
uint32_t autoLastSampleUs = 0;

uint32_t autoSamples = 0;
uint32_t autoMinSampleDtUs = 0xFFFFFFFF;
uint32_t autoMaxSampleDtUs = 0;
uint32_t autoMaxSensorUs = 0;
uint32_t autoMaxControlUs = 0;

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

  if (countForAutoStats && autoMoveActive) {
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
  Serial.print(pwmManual);

  Serial.print("  auto=");
  Serial.print(autoMoveActive ? "ON" : "OFF");

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
  driverDisable();
}

void motorRightContinuous() {
  driverEnable();

  // RIGHT / REWIND logico: baja angulo.
  // Mantener este mapeo porque fue validado en v3.0.
  ledcWrite(AIN1_PIN, pwmManual);
  ledcWrite(AIN2_PIN, 0);

  motorState = MOTOR_RIGHT;
}

void motorLeftContinuous() {
  driverEnable();

  // LEFT / FORWARD logico: sube angulo.
  // Mantener este mapeo porque fue validado en v3.0.
  ledcWrite(AIN1_PIN, 0);
  ledcWrite(AIN2_PIN, pwmManual);

  motorState = MOTOR_LEFT;
}

void commandRightManual() {
  Serial.println("CMD MOTOR RIGHT MANUAL");
  motorRightContinuous();
  lastManualMoveMs = millis();
}

void commandLeftManual() {
  Serial.println("CMD MOTOR LEFT MANUAL");
  motorLeftContinuous();
  lastManualMoveMs = millis();
}

void checkMotorTimeout() {
  if (autoMoveActive) {
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
  Serial.println(pwmManual);

  Serial.print("direction=");
  Serial.println(autoDirectionName());

  Serial.print("run_ms=");
  Serial.println(runMs);

  Serial.print("samples=");
  Serial.println(autoSamples);

  Serial.print("control_period_us=");
  Serial.println(CONTROL_PERIOD_US);

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
  Serial.println(AUTO_STALL_MAX_COUNT);

  Serial.println("==================================");
  Serial.println();
}

// ============================================================
// CONTROL AUTOMATICO CONTINUO
// ============================================================

void finishAutoMove(const char* reason, float decisionDeg, float decisionErrorDeg) {
  autoDecisionDeg = decisionDeg;
  autoDecisionErrorDeg = decisionErrorDeg;

  autoMoveActive = false;

  stopMotorOnly();

  delay(AUTO_FINAL_SETTLE_MS);

  float finalDeg = readSensorDegMeasured(false);
  float finalError = angleErrorDeg(finalDeg, autoTargetDeg);

  printAutoSummary(reason, finalDeg, finalError);
}

void cancelAutoMove(const char* reason, float decisionDeg, float decisionErrorDeg) {
  autoDecisionDeg = decisionDeg;
  autoDecisionErrorDeg = decisionErrorDeg;

  autoMoveActive = false;

  stopMotorOnly();

  delay(AUTO_FINAL_SETTLE_MS);

  float finalDeg = readSensorDegMeasured(false);
  float finalError = angleErrorDeg(finalDeg, autoTargetDeg);

  Serial.print("AUTO CANCELADO: ");
  Serial.println(reason);

  printAutoSummary(reason, finalDeg, finalError);
}

void startAutoMove(float targetDeg, const char* targetName) {
  if (autoMoveActive) {
    Serial.println("AUTO ya esta activo. Use x para cancelar.");
    return;
  }

  readSensorDegMeasured(false);

  autoTargetDeg = normalize360(targetDeg);
  autoTargetName = targetName;

  autoStartDeg = lastDeg;
  autoStartErrorDeg = angleErrorDeg(autoStartDeg, autoTargetDeg);

  autoDecisionDeg = autoStartDeg;
  autoDecisionErrorDeg = autoStartErrorDeg;

  autoStartMs = millis();
  autoLastStallCheckMs = millis();
  autoLastDebugMs = millis();

  autoLastStallDeg = autoStartDeg;
  autoStallCount = 0;

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
  Serial.println(pwmManual);

  if (fabs(autoStartErrorDeg) <= AUTO_TOLERANCE_DEG) {
    autoMoveActive = false;
    autoDirection = AUTO_DIR_NONE;

    printAutoSummary("ya_en_posicion", autoStartDeg, autoStartErrorDeg);
    return;
  }

  autoMoveActive = true;

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
  autoLastControlUs = micros() - CONTROL_PERIOD_US;
}

void updateAutoMove() {
  if (!autoMoveActive) {
    return;
  }

  uint32_t nowUs = micros();

  if ((uint32_t)(nowUs - autoLastControlUs) < CONTROL_PERIOD_US) {
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
    cancelAutoMove("FC_L_ACTIVO", currentDeg, errorDeg);
    return;
  }

  if (millis() - autoStartMs >= AUTO_MAX_RUN_MS) {
    cancelAutoMove("tiempo_maximo_alcanzado", currentDeg, errorDeg);
    return;
  }

  // Caso ideal: entro dentro de tolerancia.
  if (absErrorDeg <= AUTO_TOLERANCE_DEG) {
    finishAutoMove("posicion_alcanzada", currentDeg, errorDeg);
    return;
  }

  // Si cruce el objetivo, corto.
  // RIGHT baja angulo: el error deberia pasar de positivo a negativo.
  if (autoDirection == AUTO_DIR_RIGHT && errorDeg < -AUTO_CROSS_MARGIN_DEG) {
    finishAutoMove("objetivo_cruzado", currentDeg, errorDeg);
    return;
  }

  // LEFT sube angulo: el error deberia pasar de negativo a positivo.
  if (autoDirection == AUTO_DIR_LEFT && errorDeg > AUTO_CROSS_MARGIN_DEG) {
    finishAutoMove("objetivo_cruzado", currentDeg, errorDeg);
    return;
  }

  // Deteccion de sin movimiento.
  if (millis() - autoLastStallCheckMs >= AUTO_STALL_CHECK_MS) {
    float movedDeg = angleDistanceDeg(currentDeg, autoLastStallDeg);

    if (movedDeg < AUTO_MIN_MOVE_DEG) {
      autoStallCount++;
    } else {
      autoStallCount = 0;
    }

    autoLastStallDeg = currentDeg;
    autoLastStallCheckMs = millis();

    if (autoStallCount >= AUTO_STALL_MAX_COUNT) {
      cancelAutoMove("sin_movimiento_detectado", currentDeg, errorDeg);
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
    Serial.println(pwmManual);
  }

  uint32_t controlUs = micros() - controlStartUs;

  if (controlUs > autoMaxControlUs) {
    autoMaxControlUs = controlUs;
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
  Serial.println(pwmManual);

  Serial.print("CONTROL_PERIOD_US=");
  Serial.println(CONTROL_PERIOD_US);

  Serial.print("AUTO_TOLERANCE_DEG=");
  Serial.println(AUTO_TOLERANCE_DEG, 2);

  Serial.print("AUTO_MAX_RUN_MS=");
  Serial.println(AUTO_MAX_RUN_MS);

  Serial.print("AUTO_STALL_CHECK_MS=");
  Serial.println(AUTO_STALL_CHECK_MS);

  Serial.print("AUTO_MIN_MOVE_DEG=");
  Serial.println(AUTO_MIN_MOVE_DEG, 2);

  Serial.print("AUTO_STALL_MAX_COUNT=");
  Serial.println(AUTO_STALL_MAX_COUNT);

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
  Serial.print(pos1Deg, 2);
  Serial.print(" deg  hardcoded=");
  Serial.println(POS_1_DEFAULT_DEG, 2);

  Serial.print("POS_2=");
  Serial.print(pos2Deg, 2);
  Serial.print(" deg  hardcoded=");
  Serial.println(POS_2_DEFAULT_DEG, 2);

  Serial.print("POS_3=");
  Serial.print(pos3Deg, 2);
  Serial.print(" deg  hardcoded=");
  Serial.println(POS_3_DEFAULT_DEG, 2);

  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println("============================================================");
  Serial.println(APP_VERSION);
  Serial.println("COMANDOS:");
  Serial.println("  0 -> imprimir version/configuracion");
  Serial.println("  h -> ayuda");
  Serial.println("  p -> imprimir posicion actual una vez");
  Serial.println("  s -> stream ON/OFF, solo cuando no hay automatico");
  Serial.println("  d -> debug automatico compacto ON/OFF");
  Serial.println();
  Serial.println("  f -> pulso manual FORWARD logico, sube angulo");
  Serial.println("  r -> pulso manual REWIND logico, baja angulo");
  Serial.println("  x -> stop / cancelar automatico");
  Serial.println();
  Serial.println("  + -> subir PWM");
  Serial.println("  - -> bajar PWM");
  Serial.println("  w -> mostrar PWM actual");
  Serial.println();
  Serial.println("  a -> guardar posicion actual como POS_1 en RAM");
  Serial.println("  b -> guardar posicion actual como POS_2 en RAM");
  Serial.println("  c -> guardar posicion actual como POS_3 en RAM");
  Serial.println("  q -> mostrar posiciones");
  Serial.println();
  Serial.println("  1 -> ir continuo silencioso a POS_1");
  Serial.println("  2 -> ir continuo silencioso a POS_2");
  Serial.println("  3 -> ir continuo silencioso a POS_3");
  Serial.println("============================================================");
  Serial.println();
}

void handleCommand(char cmd) {
  if (cmd == '\n' || cmd == '\r' || cmd == ' ') {
    return;
  }

  Serial.print("RECIBI COMANDO: ");
  Serial.println(cmd);

  if (autoMoveActive && cmd != 'x') {
    Serial.println("AUTO activo: solo se acepta x para cancelar.");
    return;
  }

  switch (cmd) {
    case '0':
      printVersion();
      break;

    case 'h':
    case 'H':
      printHelp();
      break;

    case 'p':
    case 'P':
      readSensorDegMeasured(false);
      printSensor();
      break;

    case 's':
    case 'S':
      streamEnabled = !streamEnabled;
      Serial.print("stream=");
      Serial.println(streamEnabled ? "ON" : "OFF");
      break;

    case 'd':
    case 'D':
      debugAuto = !debugAuto;
      Serial.print("debugAuto=");
      Serial.println(debugAuto ? "ON" : "OFF");
      break;

    case 'f':
    case 'F':
      commandLeftManual();
      break;

    case 'r':
    case 'R':
      commandRightManual();
      break;

    case 'x':
    case 'X':
      if (autoMoveActive) {
        readSensorDegMeasured(false);
        cancelAutoMove("cancelado_por_usuario", lastDeg, angleErrorDeg(lastDeg, autoTargetDeg));
      } else {
        Serial.println("STOP manual");
        stopMotorOnly();
        readSensorDegMeasured(false);
        printSensor();
      }
      break;

    case '+':
      pwmManual += PWM_STEP;
      if (pwmManual > PWM_MAX) {
        pwmManual = PWM_MAX;
      }
      Serial.print("PWM actual=");
      Serial.println(pwmManual);
      break;

    case '-':
      pwmManual -= PWM_STEP;
      if (pwmManual < PWM_MIN) {
        pwmManual = PWM_MIN;
      }
      Serial.print("PWM actual=");
      Serial.println(pwmManual);
      break;

    case 'w':
    case 'W':
      Serial.print("PWM actual=");
      Serial.println(pwmManual);
      break;

    case 'a':
    case 'A':
      readSensorDegMeasured(false);
      pos1Deg = lastDeg;
      Serial.print("Guardada POS_1=");
      Serial.println(pos1Deg, 2);
      break;

    case 'b':
    case 'B':
      readSensorDegMeasured(false);
      pos2Deg = lastDeg;
      Serial.print("Guardada POS_2=");
      Serial.println(pos2Deg, 2);
      break;

    case 'c':
    case 'C':
      readSensorDegMeasured(false);
      pos3Deg = lastDeg;
      Serial.print("Guardada POS_3=");
      Serial.println(pos3Deg, 2);
      break;

    case 'q':
    case 'Q':
      printPositions();
      break;

    case '1':
      startAutoMove(pos1Deg, "POS_1");
      break;

    case '2':
      startAutoMove(pos2Deg, "POS_2");
      break;

    case '3':
      startAutoMove(pos3Deg, "POS_3");
      break;

    default:
      Serial.println("Comando no reconocido. Use h para ayuda.");
      break;
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    handleCommand(cmd);
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  pinMode(STBY_PIN, OUTPUT);
  pinMode(FC_L_PIN, INPUT_PULLUP);

  digitalWrite(STBY_PIN, LOW);
  driverEnabled = false;

  ledcAttach(AIN1_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(AIN2_PIN, PWM_FREQ, PWM_RES);

  motorCoast();

  SPI.begin(AS5048_SCK, AS5048_MISO, AS5048_MOSI, AS5048_CS);
  sensor.init(&SPI);

  printHelp();

  delay(500);

  readSensorDegMeasured(false);
  printSensor();

  Serial.println("Sistema listo. Stream apagado por defecto.");
}

void loop() {
  readSerialCommands();

  if (autoMoveActive) {
    updateAutoMove();
    return;
  }

  checkMotorTimeout();

  if (streamEnabled && millis() - lastStreamMs >= STREAM_PERIOD_MS) {
    lastStreamMs = millis();

    readSensorDegMeasured(false);
    printSensor();
  }
}