// C:\Users\ptavolaro\AppData\Local\arduino\sketches

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/as5048a/MagneticSensorAS5048A.h"
#include "door_config.h"
#include "door_motion.h"

/*
  ============================================================
  PROYECTO: ESP32 MOTORIZED DOOR CONTROLLER
  VERSION: v3.1-continuous-silent-measured-config-json-door-motion-step4

  OBJETIVO DE ESTA VERSION
  ------------------------------------------------------------
  Refactor conservador de la version step3 validada.

  Se separa el bloque de movimiento/posicionamiento automatico
  en CDoorMotion, manteniendo el main como coordinador del
  producto/dispositivo.

  No cambia el comportamiento fisico validado:
    - mismos pines
    - mismos sentidos
    - mismo PWM fijo configurable por JSON
    - mismos criterios de llegada/cruce/stall/timeout
    - mismo periodo de control configurado
    - mismo silencio durante el movimiento automatico
    - mismo summary final

  Concepto de arquitectura:
    main / producto:
      - host JSON
      - configuracion
      - decision de aceptar go/stop
      - estado general del dispositivo

    door_motion:
      - maquina de posicionamiento
      - start/update/cancel
      - error angular
      - llegada/cruce/stall/timeout
      - summary

    PID:
      - todavia NO implementado.
      - cuando entre, pertenece al bloque de movimiento/posicionamiento.
  ============================================================
*/

// ============================================================
// VERSION
// ============================================================

#define APP_VERSION "v3.1-continuous-silent-measured-config-json-door-motion-step4"

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
#define POS_1_DEFAULT_DEG 2.29f
#define POS_2_DEFAULT_DEG 291.23f
#define POS_3_DEFAULT_DEG 206.06f

// Stream manual de posicion.
// Por defecto apagado para no cargar Serial.
#define STREAM_PERIOD_MS 100UL

// ============================================================
// SENSOR / CONFIG / MOTION
// ============================================================

MagneticSensorAS5048A sensor(AS5048_CS);

CDoorConfig Config;
CDoorMotion DoorMotion;

// ============================================================
// ESTADOS DEL PRODUCTO / SALIDA FISICA
// ============================================================

/*
  DeviceState representa el estado general del producto.
  DoorMotion tiene su propia maquina interna de posicionamiento.

  MotorState no decide nada: solo refleja la salida fisica aplicada
  al DRV8833.
*/

enum MotorState {
  MOTOR_STOPPED,
  MOTOR_RIGHT,
  MOTOR_LEFT
};

enum DeviceState {
  DEV_IDLE,
  DEV_MANUAL_MOVING,
  DEV_POSITIONING
};

MotorState motorState = MOTOR_STOPPED;
DeviceState deviceState = DEV_IDLE;

bool driverEnabled = false;
bool streamEnabled = false;

// Ultima lectura del sensor
float lastDeg = 0.0f;
float lastRad = 0.0f;
float lastVelRadS = 0.0f;
uint16_t lastRaw = 0;
uint32_t lastSensorReadUs = 0;

// Manual
unsigned long lastManualMoveMs = 0;

// Stream
unsigned long lastStreamMs = 0;

bool isPositionActive() {
  return DoorMotion.is_active();
}

// ============================================================
// UTILIDADES ANGULARES
// ============================================================

float normalize360(float deg) {
  return CDoorMotion::normalize360(deg);
}

float angleErrorDeg(float currentDeg, float targetDeg) {
  return CDoorMotion::angle_error_deg(currentDeg, targetDeg);
}

float angleDistanceDeg(float aDeg, float bDeg) {
  return CDoorMotion::angle_distance_deg(aDeg, bDeg);
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

// ============================================================
// SENSOR
// ============================================================

float readSensorDegMeasured(bool countForMotionStats) {
  (void)countForMotionStats;

  uint32_t t0 = micros();

  sensor.update();

  lastSensorReadUs = micros() - t0;

  lastRad = sensor.getAngle();
  lastDeg = normalize360(lastRad * 57.2957795f);
  lastVelRadS = sensor.getVelocity();
  lastRaw = 0;

  return lastDeg;
}

uint32_t getLastSensorReadUs() {
  return lastSensorReadUs;
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
  Serial.print(DoorMotion.state_name());

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

void stopMotorOutputOnly() {
  motorStopBrake();
  motorState = MOTOR_STOPPED;
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
// COMANDOS / INFO LOCAL
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
  Serial.println(DoorMotion.state_name());

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
  Serial.println(DoorMotion.get_debug() ? "ON" : "OFF");

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
  Serial.println("  {\"info\":\"version\"}");
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

// ============================================================
// HOST REQUESTS: main coordina, DoorMotion mueve
// ============================================================

void startDoorMotionTo(uint8_t pos, const char* targetName) {
  if (DoorMotion.start(Config.get_pos_deg(pos), targetName)) {
    deviceState = DEV_POSITIONING;
  }
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
      if (DoorMotion.is_moving_or_starting()) {
        DoorMotion.cancel("cancelado_por_host");
      } else if (DoorMotion.is_settling()) {
        // Ya se corto el motor y se esta esperando la lectura final estable.
        // No se cambia el reason original.
      } else {
        stopMotorOnly();
      }
      break;

    case DOOR_REQ_GO_POS_1:
      startDoorMotionTo(1, "POS_1");
      break;

    case DOOR_REQ_GO_POS_2:
      startDoorMotionTo(2, "POS_2");
      break;

    case DOOR_REQ_GO_POS_3:
      startDoorMotionTo(3, "POS_3");
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

  Config.set_app_version(APP_VERSION);

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

  DoorMotionCallbacks motionCallbacks;
  motionCallbacks.read_sensor_deg = readSensorDegMeasured;
  motionCallbacks.get_last_sensor_read_us = getLastSensorReadUs;
  motionCallbacks.is_fc_l_active = isFcLActive;
  motionCallbacks.motor_right_continuous = motorRightContinuous;
  motionCallbacks.motor_left_continuous = motorLeftContinuous;
  motionCallbacks.stop_motor_output_only = stopMotorOutputOnly;
  DoorMotion.begin(&Config, motionCallbacks);

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

  if (DoorMotion.is_active()) {
    DoorMotion.update();

    if (!DoorMotion.is_active() && deviceState == DEV_POSITIONING) {
      deviceState = DEV_IDLE;
    }

    return;
  }

  if (deviceState == DEV_POSITIONING) {
    deviceState = DEV_IDLE;
  }

  checkMotorTimeout();

  if (streamEnabled && millis() - lastStreamMs >= STREAM_PERIOD_MS) {
    lastStreamMs = millis();

    readSensorDegMeasured(false);
    printSensor();
  }
}
