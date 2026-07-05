// C:\Users\ptavolaro\AppData\Local\arduino\sketches

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/as5048a/MagneticSensorAS5048A.h"

#include "door_config.h"
#include "door_motion.h"
#include "door_motor.h"
#include "door_angle_sensor.h"
#include "log.h"

/*
  ============================================================
  PROYECTO: ESP32 MOTORIZED DOOR CONTROLLER
  VERSION: v4.1c-pid-moving-no-hold

  OBJETIVO DE ESTA VERSION
  ------------------------------------------------------------
  Refactor conservador de la version step5 validada.

  Se mantiene CDoorMotion como modulo de movimiento/posicionamiento
  automatico, CDoorMotor como salida fisica DRV8833/PWM y se
  extrae la lectura AS5048A a CDoorAngleSensor, manteniendo el main
  como coordinador del producto/dispositivo.

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

    door_motor:
      - salida fisica DRV8833
      - STBY / AIN1 / AIN2
      - PWM / LEFT / RIGHT / STOP

    door_angle_sensor:
      - entrada fisica AS5048A por SPI
      - lectura angular rad/deg
      - velocidad y tiempo sensor_us

    PD/PID:
      - v4.1c habilita motion_mode=2 con PID de posicion.
      - Ki se usa en movimiento con acumulador limitado.
      - HOLDING sigue reservado para una version posterior.

    v4.1c:
      - motion_mode=0 y motion_mode=1 mantienen el flujo validado.
      - motion_mode=2 usa PID para llegar a posicion y cortar.
      - HOLDING queda reservado; todavia no se mantiene posicion.
  ============================================================
*/
/*
  ============================================================
  COMANDOS DE PRUEBA - STEP8F PLANT TRAVEL PLOTTER
  ============================================================

  Objetivo:
    Graficar respuesta de planta para un PWM fijo.

  Variables graficadas:
    travel = recorrido realizado desde el inicio del movimiento
    target = recorrido total requerido para llegar al setpoint

  Lectura:
    travel arranca en 0 y sube hacia target.
    La pendiente de travel representa la velocidad angular aproximada.
    Para comparar PWM, repetir el mismo movimiento con pwm_move distinto.

  Importante:
    Usar solo motion_mode=0 para identificación de planta.
    Configurar con log_level=1.
    Graficar con log_level=3.
    En log_level=3 no salen ACK ni summaries para no ensuciar el Plotter.

  Ver parametros:
    {"log_level":1}
    {"info":"all-params"}

  Ensayo PWM 70:
    {"log_level":1}
    {"motion_mode":0}
    {"pwm_move":70}
    {"info":"all-params"}
    {"log_level":3}
    {"cmd":"go","pos":3}

  Ensayo PWM 75:
    {"log_level":1}
    {"pwm_move":75}
    {"info":"all-params"}
    {"log_level":3}
    {"cmd":"go","pos":1}

  Ensayo PWM 80:
    {"log_level":1}
    {"pwm_move":80}
    {"info":"all-params"}
    {"log_level":3}
    {"cmd":"go","pos":3}

  Tiempo aproximado:
    tiempo_ms = cantidad_de_muestras * PLANT_PLOT_PERIOD_MS

  ============================================================
*/
// ============================================================
// VERSION
// ============================================================

#define APP_VERSION "v4.1c-pid-moving-no-hold"

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

// Plotter de planta/control.
// En motion_mode=0:
//   travel vs tiempo de muestra permite ver la respuesta de planta.
//   tiempo aproximado = cantidad de muestras * PLANT_PLOT_PERIOD_MS.
// Periodo desacoplado del periodo de control para no cargar Serial.
#define PLANT_PLOT_PERIOD_MS 50UL

// ============================================================
// SENSOR / CONFIG / MOTION
// ============================================================

MagneticSensorAS5048A sensor(AS5048_CS);

CDoorConfig Config;
CDoorMotion DoorMotion;
CDoorMotor DoorMotor;
CDoorAngleSensor DoorSensor(sensor);
Clog Log;

// ============================================================
// ESTADO DEL PRODUCTO
// ============================================================

/*
  DeviceState representa el estado general del producto.
  DoorMotion tiene su propia maquina interna de posicionamiento.
  DoorMotor solo refleja/ejecuta la salida fisica aplicada al DRV8833.
  DoorSensor encapsula la lectura fisica AS5048A.
*/

enum DeviceState {
  DEV_IDLE,
  DEV_MANUAL_MOVING,
  DEV_POSITIONING
};

DeviceState deviceState = DEV_IDLE;

bool streamEnabled = false;

// Manual
unsigned long lastManualMoveMs = 0;

// Stream
unsigned long lastStreamMs = 0;

// Plotter de planta
unsigned long lastPlantPlotMs = 0;
float plantPlotTargetDeg = 0.0f;
float plantPlotStartAbsErrorDeg = 0.0f;
uint16_t plantPlotPwmCmd = 0;

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

  return DoorSensor.read_deg();
}

uint32_t getLastSensorReadUs() {
  return DoorSensor.last_read_us();
}

bool isFcLActive() {
  return digitalRead(FC_L_PIN) == HIGH;
}

void syncLogLevel() {
  Log.set_level((uint8_t)Config.get_log_level());
}

void printSensor() {
  Log.msg(
    F("raw=%u deg=%.2f mid=%.2f delta_mid=%.2f rad=%.5f vel_rad_s=%.5f motor=%s pwm=%lu auto=%s device=%s position=%s FC_L=%s"),
    DoorSensor.raw(),
    DoorSensor.deg(),
    POS_MEDIO_APROX_DEG,
    angleErrorDeg(DoorSensor.deg(), POS_MEDIO_APROX_DEG),
    DoorSensor.rad(),
    DoorSensor.vel_rad_s(),
    DoorMotor.state_name(),
    (unsigned long)Config.get_pwm_move(),
    isPositionActive() ? "ON" : "OFF",
    deviceStateName(),
    DoorMotion.state_name(),
    isFcLActive() ? "ACTIVO" : "NORMAL"
  );
}

void plantPlotIfNeeded() {
  if (Config.get_log_level() != LOG_CTRL_ARDUINO_PLOTTER) {
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - lastPlantPlotMs < PLANT_PLOT_PERIOD_MS) {
    return;
  }

  lastPlantPlotMs = nowMs;

  // No se fuerza una lectura extra del AS5048A.
  // DoorMotion.update() ya actualiza DoorSensor.deg() durante el movimiento.
  float currentDeg = DoorSensor.deg();
  float errorDeg = angleErrorDeg(currentDeg, plantPlotTargetDeg);
  float absErrorDeg = fabs(errorDeg);

  float travelDeg = plantPlotStartAbsErrorDeg - absErrorDeg;

  if (travelDeg < 0.0f) {
    travelDeg = 0.0f;
  }

  Log.plant_plot(
    travelDeg,
    plantPlotStartAbsErrorDeg
  );
}
// ============================================================
// MOTOR / DRV8833
// ============================================================

void stopMotorOnly() {
  DoorMotor.stop_only();
  deviceState = DEV_IDLE;
}

void stopMotorOutputOnly() {
  DoorMotor.stop_output_only();
}

void motorRightContinuous(uint8_t pwm) {
  DoorMotor.right_continuous(pwm);
}

void motorLeftContinuous(uint8_t pwm) {
  DoorMotor.left_continuous(pwm);
}

void commandRightManual() {
  Log.msg(F("CMD MOTOR RIGHT MANUAL"));
  deviceState = DEV_MANUAL_MOVING;
  motorRightContinuous((uint8_t)Config.get_pwm_move());
  lastManualMoveMs = millis();
}

void commandLeftManual() {
  Log.msg(F("CMD MOTOR LEFT MANUAL"));
  deviceState = DEV_MANUAL_MOVING;
  motorLeftContinuous((uint8_t)Config.get_pwm_move());
  lastManualMoveMs = millis();
}

void checkMotorTimeout() {
  if (isPositionActive()) {
    return;
  }

  if (DoorMotor.is_stopped()) {
    return;
  }

  if (millis() - lastManualMoveMs >= TIMEOUT_MANUAL_MS) {
    Log.msg(F("AUTO STOP por timeout manual"));
    stopMotorOnly();

    readSensorDegMeasured(false);
    printSensor();
  }
}

// ============================================================
// HOST REQUESTS: main coordina, DoorMotion mueve
// ============================================================

void startDoorMotionTo(uint8_t pos, const char* targetName) {
  float targetDeg = Config.get_pos_deg(pos);

  if (DoorMotion.start(targetDeg, targetName)) {
    plantPlotTargetDeg = targetDeg;

    // DoorMotion.start() ya leyo el sensor mediante callback.
    // Usamos el valor actual del wrapper para fijar el error inicial
    // de la grafica relativa.
    plantPlotStartAbsErrorDeg = fabs(angleErrorDeg(DoorSensor.deg(), plantPlotTargetDeg));

    // Para identificacion de planta usar motion_mode=0.
    // En ese modo pwm_cmd coincide con la entrada fija aplicada.
    plantPlotPwmCmd = (uint16_t)Config.get_pwm_move();

    lastPlantPlotMs = 0;

    deviceState = DEV_POSITIONING;
  }
}

void processHostRequest() {
  if (!Config.has_request()) {
    return;
  }

  DoorHostRequest req = Config.get_request();
  Config.clear_request();

  if (DoorMotion.is_busy() && req != DOOR_REQ_STOP) {
    Log.msg(F("AUTO ocupado: solo se acepta stop para cancelar."));
    return;
  }

  switch (req) {
    case DOOR_REQ_STOP:
      if (DoorMotion.is_moving_or_starting()) {
        DoorMotion.cancel("cancelado_por_host");
      } else if (DoorMotion.is_settling()) {
        // Ya se corto el motor y se esta esperando la lectura final estable.
        // No se cambia el reason original.
      } else if (DoorMotion.is_holding()) {
        // Reservado para futuro motion_mode=2: stop libera el mantenimiento.
        DoorMotion.cancel("hold_liberado_por_host");
        stopMotorOnly();
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

  syncLogLevel();

  pinMode(FC_L_PIN, INPUT_PULLUP);

  DoorMotor.begin(STBY_PIN, AIN1_PIN, AIN2_PIN, PWM_FREQ, PWM_RES);
  deviceState = DEV_IDLE;

  // Mantener inicializacion AS5048A exactamente igual a Step 5 validado.
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

  if (Config.get_log_level() != LOG_CTRL_ARDUINO_PLOTTER) {
    Serial.println();
    Serial.print("BOOT OK - ");
    Serial.println(APP_VERSION);
    Log.msg(F("Host: JSON only. Ejemplo: {\"info\":\"all-params\"}"));
  }

  delay(200);

  readSensorDegMeasured(false);

  if (Config.get_log_level() != LOG_CTRL_ARDUINO_PLOTTER) {
    printSensor();
    Log.msg(F("Sistema listo. Stream apagado por defecto."));
  }
}

void loop() {
  Config.host_cmd();
  syncLogLevel();
  processHostRequest();

  if (DoorMotion.is_active()) {
    DoorMotion.update();
    plantPlotIfNeeded();

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
