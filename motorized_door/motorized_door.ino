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
#include "door_led_strip.h"
#include "timer.h"
#include "button.h"
#include "log.h"

/*
  ============================================================
  PROYECTO: ESP32 MOTORIZED DOOR CONTROLLER
  VERSION: v5.2b-danger-button-fsm
  ============================================================

  ALCANCE ACTUAL
  ------------------------------------------------------------
  - ESP32-S3 + DRV8833 + motor N20 6 V.
  - AS5048A por SPI como realimentacion angular.
  - Posicionamiento modular mediante CDoorMotion.
  - Control configurable por motion_mode.
  - Default validado para el motor nuevo: motion_mode=2,
    Kp=0.7, Ki=0, Kd=0, PWM maximo 80 y minimo efectivo 70.
  - FSM superior de demo:
      centra automaticamente en POS_2 al arrancar en st_mode=0;
      fwd abre a POS_1, espera y vuelve a POS_2;
      rew abre a POS_3, espera y vuelve a POS_2.
  - FSM LED WS2812B:
      IDLE azul; apertura verde; espera verde fija;
      cierre rojo desplazandose; ARRIVED suave.
  - Pulsador DANGER en GPIO14:
      se atiende solamente en DEV_READY, con puerta centrada en POS_2;
      activa rojo intermitente durante danger_time_ms y vuelve a READY.
  - led-sim runtime para diagnostico:
      usa el mismo camino DoorMotion/DoorMotor/FSM LED;
      reemplaza solamente la lectura angular por una posicion virtual.
      Para ensayar sin movimiento fisico se desconecta VM/6 V.

  ARQUITECTURA
  ------------------------------------------------------------
  motorized_door.ino
    Coordina host JSON, estado superior, movimiento, sensor y LED.

  CDoorConfig
    Configuracion RAM/NVS y protocolo JSON.

  CDoorMotion
    FSM de posicionamiento: START, MOVING, SETTLING e IDLE.

  CDoorMotor
    Salida DRV8833: STBY, AIN1, AIN2 y PWM.

  CDoorAngleSensor
    Lectura del AS5048A y medicion de tiempo de acceso.

  CLedStrip
    FSM visual no bloqueante de la tira WS2812B.

  NOTAS
  ------------------------------------------------------------
  - HOLDING activo permanece reservado para hardware real de bloqueo.
  - st_mode=100 evita el centrado automatico y reserva el equipo para pruebas.
  - open_wait_ms y danger_time_ms se cargan desde NVS a la copia RAM de CDoorConfig.
  - El problema de tironeo observado tras cambiar el motor fue aislado
    al termino D de la implementacion actual. Con Kd=0 el movimiento y
    la animacion LED quedaron validados.
  - La referencia completa de comandos y defaults vive en readme.md.
  ============================================================
*/
// ============================================================
// VERSION
// ============================================================

#define APP_VERSION "v5.2b-danger-button-fsm"

// ============================================================
// PINES
// ============================================================

// DRV8833
#define STBY_PIN 4
#define AIN2_PIN 16
#define AIN1_PIN 17

// Pulsador DANGER (hardware existente en GPIO14)
#define DANGER_BUTTON_PIN 14
#define DANGER_BUTTON_ACTIVE_LEVEL HIGH

// AS5048A SPI
#define AS5048_CS   10
#define AS5048_MOSI 11
#define AS5048_SCK  12
#define AS5048_MISO 13

// Tira RGB WS2812B
#define RGB_LED_DATA 6

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
CLedStrip LedStrip;
CButton DangerButton;
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
  DEV_BOOT,
  DEV_CENTERING,
  DEV_READY,
  DEV_OPENING_FWD,
  DEV_OPENING_REW,
  DEV_OPEN_WAIT,
  DEV_CLOSING_CENTER,
  DEV_DIAGNOSTIC_POSITIONING,
  DEV_MANUAL_MOVING,
  DEV_DANGER,
  DEV_STOPPED
};

enum DemoCycleKind : uint8_t {
  DEMO_CYCLE_NONE = 0,
  DEMO_CYCLE_FWD,
  DEMO_CYCLE_REW
};

DeviceState deviceState = DEV_BOOT;
DemoCycleKind activeDemoCycle = DEMO_CYCLE_NONE;

bool streamEnabled = false;

// Manual
CTimer manualMoveTimer;

// Espera no bloqueante con puerta abierta.
// El objeto vive en el .ino; el tiempo viene de la RAM de CDoorConfig.
CTimer openWaitTimer;
uint32_t activeOpenWaitMs = 0;

// Estado DANGER no bloqueante.
// El objeto vive en el .ino; el tiempo viene de la RAM de CDoorConfig.
CTimer dangerTimer;
uint32_t activeDangerTimeMs = 0;

// Movimiento directo de diagnostico.
uint8_t diagnosticTargetPos = 0;

// Stream
CTimer streamTimer;

// Plotter de planta
CTimer plantPlotTimer;
float plantPlotTargetDeg = 0.0f;
float plantPlotStartAbsErrorDeg = 0.0f;
uint16_t plantPlotPwmCmd = 0;

// Cache local para reconfigurar la tira solo cuando cambia algun parametro JSON/NVS.
uint32_t ledCfgEnabled = 0xFFFFFFFFUL;
uint32_t ledCfgCount = 0xFFFFFFFFUL;
uint32_t ledCfgBrightness = 0xFFFFFFFFUL;
uint32_t ledCfgStepMs = 0xFFFFFFFFUL;
uint32_t ledCfgBreathMs = 0xFFFFFFFFUL;
uint32_t ledCfgBlinkMs = 0xFFFFFFFFUL;

// led-sim usa el recorrido normal; solo reemplaza temporalmente al AS5048A.
bool ledSimActive = false;
uint8_t ledSimFromPos = 0;
uint8_t ledSimToPos = 0;
uint32_t ledSimDurationMs = 0;
uint32_t ledSimStartMs = 0;
DeviceState ledSimReturnState = DEV_STOPPED;

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
    case DEV_BOOT:
      return "BOOT";

    case DEV_CENTERING:
      return "CENTERING";

    case DEV_READY:
      return "READY";

    case DEV_OPENING_FWD:
      return "OPENING_FWD";

    case DEV_OPENING_REW:
      return "OPENING_REW";

    case DEV_OPEN_WAIT:
      return "OPEN_WAIT";

    case DEV_CLOSING_CENTER:
      return "CLOSING_CENTER";

    case DEV_DIAGNOSTIC_POSITIONING:
      return "DIAGNOSTIC_POSITIONING";

    case DEV_MANUAL_MOVING:
      return "MANUAL_MOVING";

    case DEV_DANGER:
      return "DANGER";

    case DEV_STOPPED:
    default:
      return "STOPPED";
  }
}

bool isNormalExecutionMode() {
  return Config.get_st_mode() == DOOR_ST_MODE_NORMAL;
}

bool isDemoCycleActive() {
  return deviceState == DEV_OPENING_FWD ||
         deviceState == DEV_OPENING_REW ||
         deviceState == DEV_OPEN_WAIT ||
         deviceState == DEV_CLOSING_CENTER;
}

// ============================================================
// SENSOR
// ============================================================

float readLedSimDeg() {
  float fromDeg = Config.get_pos_deg(ledSimFromPos);
  float toDeg = Config.get_pos_deg(ledSimToPos);
  float deltaDeg = angleErrorDeg(fromDeg, toDeg);
  uint32_t elapsedMs = millis() - ledSimStartMs;

  if (elapsedMs >= ledSimDurationMs) {
    return toDeg;
  }

  float progress = (float)elapsedMs / (float)ledSimDurationMs;
  return normalize360(fromDeg + deltaDeg * progress);
}

float readSensorDegMeasured(bool countForMotionStats) {
  (void)countForMotionStats;

  if (ledSimActive) {
    return readLedSimDeg();
  }

  return DoorSensor.read_deg();
}

uint32_t getLastSensorReadUs() {
  return ledSimActive ? 0 : DoorSensor.last_read_us();
}

bool isDangerSwitchActive() {
  return DangerButton.is_active();
}

void syncLogLevel() {
  Log.set_level((uint8_t)Config.get_log_level());
}

void printSensor() {
  Log.msg(
    F("raw=%u deg=%.2f mid=%.2f delta_mid=%.2f rad=%.5f vel_rad_s=%.5f motor=%s pwm=%lu auto=%s device=%s position=%s DANGER_SW=%s"),
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
    isDangerSwitchActive() ? "ACTIVO" : "NORMAL"
  );
}

void plantPlotIfNeeded() {
  if (Config.get_log_level() != LOG_CTRL_ARDUINO_PLOTTER) {
    return;
  }

  if (!plantPlotTimer.expired_ms(PLANT_PLOT_PERIOD_MS)) {
    return;
  }

  plantPlotTimer.start();

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
  manualMoveTimer.start();
}

void commandLeftManual() {
  Log.msg(F("CMD MOTOR LEFT MANUAL"));
  deviceState = DEV_MANUAL_MOVING;
  motorLeftContinuous((uint8_t)Config.get_pwm_move());
  manualMoveTimer.start();
}

void checkMotorTimeout() {
  if (isPositionActive()) {
    return;
  }

  if (DoorMotor.is_stopped()) {
    return;
  }

  if (manualMoveTimer.expired_ms(TIMEOUT_MANUAL_MS)) {
    Log.msg(F("AUTO STOP por timeout manual"));
    stopMotorOnly();
    deviceState = DEV_STOPPED;

    readSensorDegMeasured(false);
    printSensor();
  }
}

// ============================================================
// FSM SUPERIOR / HOST REQUESTS
// ============================================================

bool startDoorMotionTo(uint8_t pos, const char* targetName) {
  float targetDeg = Config.get_pos_deg(pos);

  if (!DoorMotion.start(targetDeg, targetName)) {
    return false;
  }

  plantPlotTargetDeg = targetDeg;

  // DoorMotion.start() todavia no ejecuta START, por lo que usamos
  // la ultima lectura disponible del wrapper o la posicion virtual.
  plantPlotStartAbsErrorDeg = fabs(angleErrorDeg(
    ledSimActive ? readLedSimDeg() : DoorSensor.deg(),
    plantPlotTargetDeg
  ));

  // Para identificacion de planta usar motion_mode=0.
  plantPlotPwmCmd = (uint16_t)Config.get_pwm_move();
  plantPlotTimer.start_ms_ago(PLANT_PLOT_PERIOD_MS);

  return true;
}

void setDeviceReady(const char* reason) {
  activeDemoCycle = DEMO_CYCLE_NONE;
  diagnosticTargetPos = 0;
  deviceState = DEV_READY;

  Log.msg(F("DEVICE READY: %s"), reason == nullptr ? "sin_reason" : reason);
}

void setDeviceStopped(const char* reason) {
  activeDemoCycle = DEMO_CYCLE_NONE;
  diagnosticTargetPos = 0;
  deviceState = DEV_STOPPED;

  Log.msg(F("DEVICE STOPPED: %s"), reason == nullptr ? "sin_reason" : reason);
}

void enterDanger() {
  activeDangerTimeMs = Config.get_danger_time_ms();
  dangerTimer.start();
  deviceState = DEV_DANGER;

  Log.msg(F("DEVICE DANGER: %lu ms"), (unsigned long)activeDangerTimeMs);
}

void beginOpenWait() {
  // Se toma una copia RAM al entrar al estado. Cambiar open_wait_ms
  // durante la espera afecta al ciclo siguiente, no al actual.
  activeOpenWaitMs = Config.get_open_wait_ms();
  openWaitTimer.start();
  deviceState = DEV_OPEN_WAIT;

  Log.msg(F("DEMO OPEN WAIT: %lu ms"), (unsigned long)activeOpenWaitMs);
}

bool startDemoOpening(DemoCycleKind cycle) {
  uint8_t targetPos = cycle == DEMO_CYCLE_FWD ? 1 : 3;
  const char* targetName = cycle == DEMO_CYCLE_FWD ? "DEMO_FWD_POS_1" : "DEMO_REW_POS_3";

  activeDemoCycle = cycle;
  deviceState = cycle == DEMO_CYCLE_FWD ? DEV_OPENING_FWD : DEV_OPENING_REW;

  if (!startDoorMotionTo(targetPos, targetName)) {
    setDeviceStopped("motion_start_failed");
    return false;
  }

  return true;
}

void startDemoClosingCenter() {
  deviceState = DEV_CLOSING_CENTER;

  if (!startDoorMotionTo(2, "DEMO_CLOSE_POS_2")) {
    setDeviceStopped("close_start_failed");
  }
}

void handleDoorMotionCompletion() {
  if (!DoorMotion.has_completion_event()) {
    return;
  }

  bool success = DoorMotion.completion_succeeded();
  const char* reason = DoorMotion.completion_reason();
  DeviceState completedState = deviceState;
  bool completedLedSim = ledSimActive;

  DoorMotion.clear_completion_event();

  if (completedLedSim) {
    ledSimActive = false;

    // Un stop durante led-sim deja el equipo detenido, igual que cualquier
    // otra cancelacion explicita del host.
    if (completedState == DEV_STOPPED) {
      return;
    }

    if (success) {
      deviceState = ledSimReturnState;
      Log.msg(F("LED SIM finalizado: reason=%s return=%s"), reason, deviceStateName());
    } else {
      setDeviceStopped(reason);
    }

    return;
  }

  switch (completedState) {
    case DEV_CENTERING:
      if (success) {
        setDeviceReady(reason);
      } else {
        setDeviceStopped(reason);
      }
      break;

    case DEV_OPENING_FWD:
    case DEV_OPENING_REW:
      if (success) {
        beginOpenWait();
      } else {
        setDeviceStopped(reason);
      }
      break;

    case DEV_CLOSING_CENTER:
      if (success) {
        setDeviceReady(reason);
      } else {
        setDeviceStopped(reason);
      }
      break;

    case DEV_DIAGNOSTIC_POSITIONING:
      if (success && diagnosticTargetPos == 2 && isNormalExecutionMode()) {
        setDeviceReady(reason);
      } else {
        setDeviceStopped(reason);
      }
      break;

    case DEV_STOPPED:
      // El host cancelo el ciclo mientras DoorMotion terminaba SETTLING.
      // Se consume el evento sin iniciar ninguna accion automatica.
      break;

    default:
      if (!success) {
        setDeviceStopped(reason);
      }
      break;
  }
}

void updateDeviceFsm() {
  handleDoorMotionCompletion();

  if (deviceState == DEV_READY) {
    DangerButton.debounce();

    if (DangerButton.is_pressed()) {
      enterDanger();
      return;
    }
  }

  if (deviceState == DEV_DANGER) {
    if (dangerTimer.expired_ms(activeDangerTimeMs)) {
      setDeviceReady("danger_timeout");
    }

    return;
  }

  if (deviceState == DEV_OPEN_WAIT && openWaitTimer.expired_ms(activeOpenWaitMs)) {
    startDemoClosingCenter();
  }
}

void initializeDeviceFsm() {
  deviceState = DEV_BOOT;
  activeDemoCycle = DEMO_CYCLE_NONE;

  if (!isNormalExecutionMode()) {
    setDeviceStopped("st_mode_test_no_auto_center");
    return;
  }

  float currentDeg = DoorSensor.deg();
  float centerErrorDeg = angleDistanceDeg(currentDeg, Config.get_pos2_deg());

  if (centerErrorDeg <= Config.get_auto_tolerance_deg()) {
    setDeviceReady("boot_already_centered");
    return;
  }

  deviceState = DEV_CENTERING;

  if (!startDoorMotionTo(2, "BOOT_CENTER_POS_2")) {
    setDeviceStopped("boot_center_start_failed");
  }
}

void stopAutomaticActivity() {
  activeDemoCycle = DEMO_CYCLE_NONE;
  deviceState = DEV_STOPPED;

  if (DoorMotion.is_moving_or_starting()) {
    DoorMotion.cancel("cancelado_por_host");
    return;
  }

  if (DoorMotion.is_holding()) {
    DoorMotion.cancel("hold_liberado_por_host");
    return;
  }

  // En SETTLING o OPEN_WAIT el motor ya esta cortado. No se reemplaza
  // el reason original y no se agrega ninguna espera bloqueante.
  if (!DoorMotor.is_stopped()) {
    stopMotorOnly();
  }
}

void processHostRequest() {
  if (!Config.has_request()) {
    return;
  }

  DoorHostRequest req = Config.get_request();
  uint8_t requestedFromPos = Config.get_requested_from_position();
  uint8_t requestedToPos = Config.get_requested_to_position();
  uint32_t requestedDurationMs = Config.get_requested_duration_ms();
  Config.clear_request();

  if (req == DOOR_REQ_STOP) {
    stopAutomaticActivity();
    return;
  }

  if (req == DOOR_REQ_FWD || req == DOOR_REQ_REW) {
    const char* command = req == DOOR_REQ_FWD ? "fwd" : "rew";

    if (!isNormalExecutionMode()) {
      Config.send_runtime_command_result(command, false, "st_mode_test", deviceStateName());
      return;
    }

    if (deviceState != DEV_READY || DoorMotion.is_active() || ledSimActive) {
      Config.send_runtime_command_result(command, false, "device_not_ready", deviceStateName());
      return;
    }

    DemoCycleKind cycle = req == DOOR_REQ_FWD ? DEMO_CYCLE_FWD : DEMO_CYCLE_REW;

    if (!startDemoOpening(cycle)) {
      Config.send_runtime_command_result(command, false, "motion_start_failed", deviceStateName());
      return;
    }

    Config.send_runtime_command_result(command, true, nullptr, deviceStateName());
    return;
  }

  if (DoorMotion.is_active() || isDemoCycleActive() || deviceState == DEV_CENTERING || deviceState == DEV_DANGER) {
    Log.msg(F("DEVICE ocupado: solo se acepta stop para cancelar."));
    return;
  }

  switch (req) {
    case DOOR_REQ_GO_POS_1:
    case DOOR_REQ_GO_POS_2:
    case DOOR_REQ_GO_POS_3: {
      uint8_t pos = req == DOOR_REQ_GO_POS_1 ? 1 : (req == DOOR_REQ_GO_POS_2 ? 2 : 3);
      const char* name = pos == 1 ? "POS_1" : (pos == 2 ? "POS_2" : "POS_3");

      diagnosticTargetPos = pos;

      if (startDoorMotionTo(pos, name)) {
        deviceState = DEV_DIAGNOSTIC_POSITIONING;
      } else {
        setDeviceStopped("diagnostic_start_failed");
      }
      break;
    }

    case DOOR_REQ_LED_SIM:
      if (deviceState != DEV_READY && deviceState != DEV_STOPPED) {
        Log.msg(F("LED SIM rechazado: device=%s"), deviceStateName());
        break;
      }

      ledSimReturnState = deviceState;
      ledSimFromPos = requestedFromPos;
      ledSimToPos = requestedToPos;
      ledSimDurationMs = requestedDurationMs;
      ledSimStartMs = millis();
      ledSimActive = true;
      diagnosticTargetPos = ledSimToPos;

      if (startDoorMotionTo(ledSimToPos, "LED_SIM")) {
        deviceState = DEV_DIAGNOSTIC_POSITIONING;
      } else {
        ledSimActive = false;
        deviceState = ledSimReturnState;
      }
      break;

    default:
      break;
  }
}


// ============================================================
// LED STRIP / WS2812B
// ============================================================

void syncLedStripConfig() {
  uint32_t enabled = Config.get_led_enabled();
  uint32_t count = Config.get_led_count();
  uint32_t brightness = Config.get_led_brightness();
  uint32_t stepMs = Config.get_led_step_ms();
  uint32_t breathMs = Config.get_led_breath_ms();
  uint32_t blinkMs = Config.get_led_blink_ms();

  if (enabled == ledCfgEnabled &&
      count == ledCfgCount &&
      brightness == ledCfgBrightness &&
      stepMs == ledCfgStepMs &&
      breathMs == ledCfgBreathMs &&
      blinkMs == ledCfgBlinkMs) {
    return;
  }

  ledCfgEnabled = enabled;
  ledCfgCount = count;
  ledCfgBrightness = brightness;
  ledCfgStepMs = stepMs;
  ledCfgBreathMs = breathMs;
  ledCfgBlinkMs = blinkMs;

  LedStrip.configure(enabled != 0,
                     (uint16_t)count,
                     (uint8_t)brightness,
                     stepMs,
                     breathMs,
                     blinkMs);
}

void updateLedStripStateFromRuntime() {
  if (Config.get_led_enabled() == 0) {
    LedStrip.set_state(LED_STRIP_OFF);
    return;
  }

  if (deviceState == DEV_DANGER) {
    LedStrip.set_state(LED_STRIP_ALARM);
    return;
  }

  DoorMotorState motorState = DoorMotor.state();

  if (deviceState == DEV_OPEN_WAIT) {
    LedStrip.set_state(LED_STRIP_OPEN_WAIT);
    return;
  }

  if (deviceState == DEV_CLOSING_CENTER) {
    if (motorState == DOOR_MOTOR_RIGHT) {
      LedStrip.set_state(LED_STRIP_CLOSING_RWD);
      return;
    }

    if (motorState == DOOR_MOTOR_LEFT) {
      LedStrip.set_state(LED_STRIP_CLOSING_FWD);
      return;
    }

    // START antes de aplicar salida: el ciclo indica el sentido esperado.
    LedStrip.set_state(activeDemoCycle == DEMO_CYCLE_FWD
                         ? LED_STRIP_CLOSING_RWD
                         : LED_STRIP_CLOSING_FWD);
    return;
  }

  if (DoorMotion.is_moving_or_starting() ||
      motorState == DOOR_MOTOR_LEFT ||
      motorState == DOOR_MOTOR_RIGHT) {
    if (motorState == DOOR_MOTOR_RIGHT) {
      LedStrip.set_state(LED_STRIP_MOVING_RWD);
      return;
    }

    if (motorState == DOOR_MOTOR_LEFT) {
      LedStrip.set_state(LED_STRIP_MOVING_FWD);
      return;
    }

    // START antes de aplicar salida de motor: verde generico.
    LedStrip.set_state(LED_STRIP_MOVING_FWD);
    return;
  }

  LedStrip.set_state(LED_STRIP_IDLE);
}

void updateLedStrip() {
  syncLedStripConfig();
  updateLedStripStateFromRuntime();
  LedStrip.update();
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

  DangerButton.init(DANGER_BUTTON_PIN, DANGER_BUTTON_ACTIVE_LEVEL, INPUT_PULLUP);

  LedStrip.begin(RGB_LED_DATA,
                 (uint16_t)Config.get_led_count(),
                 (uint8_t)Config.get_led_brightness());
  syncLedStripConfig();
  LedStrip.set_state(LED_STRIP_IDLE);
  LedStrip.update();

  DoorMotor.begin(STBY_PIN, AIN1_PIN, AIN2_PIN, PWM_FREQ, PWM_RES);
  deviceState = DEV_BOOT;

  // Mantener la secuencia de inicializacion AS5048A ya validada.
  SPI.begin(AS5048_SCK, AS5048_MISO, AS5048_MOSI, AS5048_CS);
  sensor.init(&SPI);

  DoorMotionCallbacks motionCallbacks;
  motionCallbacks.read_sensor_deg = readSensorDegMeasured;
  motionCallbacks.get_last_sensor_read_us = getLastSensorReadUs;
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
    Log.msg(F("Inicializacion completa. Stream apagado por defecto."));
  }

  initializeDeviceFsm();
}

void loop() {
  Config.host_cmd();
  syncLogLevel();
  processHostRequest();

  if (DoorMotion.is_active()) {
    DoorMotion.update();
    plantPlotIfNeeded();
  }

  updateDeviceFsm();
  checkMotorTimeout();
  updateLedStrip();

  if (streamEnabled && streamTimer.expired_ms(STREAM_PERIOD_MS)) {
    streamTimer.start();

    readSensorDegMeasured(false);
    printSensor();
  }
}
