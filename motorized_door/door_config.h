#ifndef DOOR_CONFIG_H
#define DOOR_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

/*
  ============================================================
  CDoorConfig
  ------------------------------------------------------------
  Responsabilidad:
    - Configuracion persistente en NVS mediante Preferences.
    - Copia RAM de parametros.
    - Comunicacion host por Serial + ArduinoJson.
    - Pedidos pendientes para que el main decida.

  Regla de arquitectura:
    - Entrada host: SOLO JSON por Serial.
    - Salida host de la clase: SOLO JSON por Serial.
    - NO procesa caracteres sueltos.
    - NO mueve motor.
    - NO lee sensor.
    - NO ejecuta la maquina de estados.
    - NO hace Serial.begin().
  ============================================================
*/

// ============================================================
// NVS
// ============================================================

#define DOOR_CFG_NAMESPACE        "doorcfg"
#define DOOR_CFG_MAGIC_NUMBER     974UL
#define DOOR_CFG_SCHEMA_VERSION   1UL

// ============================================================
// DEFAULTS VALIDADOS - v5.1e / MOTOR N20 NUEVO + LED FSM
// ============================================================

#define DOOR_POS_1_DEFAULT_DEG    2.29f
#define DOOR_POS_2_DEFAULT_DEG    291.23f
#define DOOR_POS_3_DEFAULT_DEG    206.06f

#define DOOR_PWM_MOVE_DEFAULT     80UL

#define DOOR_CONTROL_PERIOD_US_DEFAULT        5000UL
#define DOOR_AUTO_TOLERANCE_DEG_DEFAULT       2.00f
#define DOOR_AUTO_CROSS_MARGIN_DEG_DEFAULT    0.20f
#define DOOR_AUTO_MAX_RUN_MS_DEFAULT          10000UL
#define DOOR_AUTO_STALL_CHECK_MS_DEFAULT      250UL
#define DOOR_AUTO_MIN_MOVE_DEG_DEFAULT        0.30f
#define DOOR_AUTO_STALL_MAX_COUNT_DEFAULT     3UL
#define DOOR_AUTO_FINAL_SETTLE_MS_DEFAULT     80UL

#define DOOR_LOG_LEVEL_DISABLED             0UL
#define DOOR_LOG_LEVEL_MSG                  1UL
#define DOOR_LOG_LEVEL_JSON                 2UL
#define DOOR_LOG_LEVEL_PLOTTER              3UL

#define DOOR_LOG_LEVEL_DEFAULT              DOOR_LOG_LEVEL_MSG

// Tira RGB WS2812B
#define DOOR_LED_ENABLED_DEFAULT            1UL
#define DOOR_LED_COUNT_DEFAULT              8UL
#define DOOR_LED_COUNT_MAX                  60UL
#define DOOR_LED_BRIGHTNESS_DEFAULT         25UL
#define DOOR_LED_STEP_MS_DEFAULT            60UL
#define DOOR_LED_BREATH_MS_DEFAULT          25UL
#define DOOR_LED_BLINK_MS_DEFAULT           180UL

// Movimiento:
//   0 = baseline fixed PWM.
//   1 = perfil no-PID de aproximacion.
//   2 = control de posicion P/PI/PD/PID durante MOVING, sin HOLDING activo.
#define DOOR_MOTION_MODE_FIXED_PWM             0UL
#define DOOR_MOTION_MODE_APPROACH              1UL
#define DOOR_MOTION_MODE_PD_POSITION           2UL
#define DOOR_MOTION_MODE_DEFAULT               DOOR_MOTION_MODE_PD_POSITION

#define DOOR_PWM_START_DEFAULT                 80UL
#define DOOR_PWM_SLOW_DEFAULT                  60UL
#define DOOR_SLOW_ZONE_DEG_DEFAULT             20.0f
#define DOOR_START_BOOST_MS_DEFAULT            120UL

// Parametros validados para motion_mode=2 con el motor N20 nuevo.
// El default actual es control P: Ki=0 y Kd=0.
#define DOOR_PID_KP_DEFAULT                    0.70f
#define DOOR_PID_KI_DEFAULT                    0.00f
#define DOOR_PID_KD_DEFAULT                    0.00f
#define DOOR_PID_PWM_MAX_DEFAULT               80UL
#define DOOR_PID_PWM_MIN_EFFECTIVE_DEFAULT     70UL
#define DOOR_PID_MIN_EFFECTIVE_ERROR_DEFAULT   2.0f
#define DOOR_PID_I_ACTIVE_ERROR_DEFAULT        20.0f
#define DOOR_PID_INTEGRAL_LIMIT_DEFAULT        120.0f

// ============================================================
// MODO / PEDIDOS HOST
// ============================================================

#define DOOR_ST_MODE_NORMAL       0UL
#define DOOR_ST_MODE_TEST         100UL

enum DoorHostRequest : uint8_t {
  DOOR_REQ_NONE = 0,
  DOOR_REQ_GO_POS_1,
  DOOR_REQ_GO_POS_2,
  DOOR_REQ_GO_POS_3,
  DOOR_REQ_STOP,
  DOOR_REQ_LED_SIM
};

class CDoorConfig {
public:
  CDoorConfig();

  bool init();
  void set_app_version(const char* version);

  /*
    Lee comunicacion por Serial.

    Protocolo host: SOLO JSON.

    Ejemplos:
      {"info":"version"}
      {"info":"all-params"}
      {"cmd":"go","pos":2}
      {"cmd":"stop"}
      {"cmd":"led-sim","from":1,"to":2,"ms":700}
      {"cmd":"led-sim-stop"}
      {"cmd":"factory-reset"}
      {"pwm_move":80}
      {"pos1_deg":2.29}
      {"pos2_deg":291.23}
      {"pos3_deg":206.06}
      {"log_level":1}
      {"motion_mode":2}
      {"pwm_start":80}
      {"pwm_slow":60}
      {"slow_zone_deg":20}
      {"start_boost_ms":120}
      {"pid_kp":0.70}
      {"pid_ki":0.00}
      {"pid_kd":0.00}
      {"pid_pwm_max":80}
      {"pid_pwm_min_effective":70}
      {"pid_min_effective_error_deg":2}
      {"pid_i_active_error_deg":20}
      {"pid_integral_limit":120}
      {"led_enabled":1}
      {"led_count":8}
      {"led_brightness":25}
      {"led_step_ms":60}
      {"led_breath_ms":25}
      {"led_blink_ms":180}
  */
  void host_cmd();

  // ==========================================================
  // Getters de configuracion persistente / copia RAM
  // ==========================================================

  float get_pos1_deg() const;
  float get_pos2_deg() const;
  float get_pos3_deg() const;
  float get_pos_deg(uint8_t pos) const;

  uint32_t get_pwm_move() const;

  uint32_t get_motion_mode() const;
  uint32_t get_pwm_start() const;
  uint32_t get_pwm_slow() const;
  float get_slow_zone_deg() const;
  uint32_t get_start_boost_ms() const;

  float get_pid_kp() const;
  float get_pid_ki() const;
  float get_pid_kd() const;
  uint32_t get_pid_pwm_max() const;
  uint32_t get_pid_pwm_min_effective() const;
  float get_pid_min_effective_error_deg() const;
  float get_pid_i_active_error_deg() const;
  float get_pid_integral_limit() const;

  uint32_t get_control_period_us() const;

  float get_auto_tolerance_deg() const;
  float get_auto_cross_margin_deg() const;
  uint32_t get_auto_max_run_ms() const;
  uint32_t get_auto_stall_check_ms() const;
  float get_auto_min_move_deg() const;
  uint32_t get_auto_stall_max_count() const;
  uint32_t get_auto_final_settle_ms() const;

  uint32_t get_log_level() const;
  uint32_t get_st_mode() const;

  uint32_t get_led_enabled() const;
  uint32_t get_led_count() const;
  uint32_t get_led_brightness() const;
  uint32_t get_led_step_ms() const;
  uint32_t get_led_breath_ms() const;
  uint32_t get_led_blink_ms() const;

  // ==========================================================
  // Setters: actualizan RAM + NVS
  // ==========================================================

  void set_pos1_deg(float value);
  void set_pos2_deg(float value);
  void set_pos3_deg(float value);

  void set_pwm_move(uint32_t value);

  void set_motion_mode(uint32_t value);
  void set_pwm_start(uint32_t value);
  void set_pwm_slow(uint32_t value);
  void set_slow_zone_deg(float value);
  void set_start_boost_ms(uint32_t value);

  void set_pid_kp(float value);
  void set_pid_ki(float value);
  void set_pid_kd(float value);
  void set_pid_pwm_max(uint32_t value);
  void set_pid_pwm_min_effective(uint32_t value);
  void set_pid_min_effective_error_deg(float value);
  void set_pid_i_active_error_deg(float value);
  void set_pid_integral_limit(float value);

  void set_control_period_us(uint32_t value);

  void set_auto_tolerance_deg(float value);
  void set_auto_cross_margin_deg(float value);
  void set_auto_max_run_ms(uint32_t value);
  void set_auto_stall_check_ms(uint32_t value);
  void set_auto_min_move_deg(float value);
  void set_auto_stall_max_count(uint32_t value);
  void set_auto_final_settle_ms(uint32_t value);

  void set_log_level(uint32_t value);
  void set_st_mode(uint32_t value);

  void set_led_enabled(uint32_t value);
  void set_led_count(uint32_t value);
  void set_led_brightness(uint32_t value);
  void set_led_step_ms(uint32_t value);
  void set_led_breath_ms(uint32_t value);
  void set_led_blink_ms(uint32_t value);

  // ==========================================================
  // Pedidos pendientes para que el main decida
  // ==========================================================

  bool has_request() const;
  DoorHostRequest get_request() const;
  uint8_t get_requested_position() const;
  uint8_t get_requested_from_position() const;
  uint8_t get_requested_to_position() const;
  uint32_t get_requested_duration_ms() const;
  void clear_request();

  // ==========================================================
  // Info / reset
  // ==========================================================

  void factory_reset();

private:
  Preferences prefs;

  const char* app_version;
  bool nvs_ready;

  // Copia RAM persistente
  float pos1_deg;
  float pos2_deg;
  float pos3_deg;

  uint32_t pwm_move;

  uint32_t motion_mode;
  uint32_t pwm_start;
  uint32_t pwm_slow;
  float slow_zone_deg;
  uint32_t start_boost_ms;

  float pid_kp;
  float pid_ki;
  float pid_kd;
  uint32_t pid_pwm_max;
  uint32_t pid_pwm_min_effective;
  float pid_min_effective_error_deg;
  float pid_i_active_error_deg;
  float pid_integral_limit;

  uint32_t control_period_us;

  float auto_tolerance_deg;
  float auto_cross_margin_deg;
  uint32_t auto_max_run_ms;
  uint32_t auto_stall_check_ms;
  float auto_min_move_deg;
  uint32_t auto_stall_max_count;
  uint32_t auto_final_settle_ms;

  uint32_t log_level;
  uint32_t st_mode;

  uint32_t led_enabled;
  uint32_t led_count;
  uint32_t led_brightness;
  uint32_t led_step_ms;
  uint32_t led_breath_ms;
  uint32_t led_blink_ms;

  // Runtime / comunicacion
  DoorHostRequest pending_request;
  uint8_t requested_position;
  uint8_t requested_from_position;
  uint8_t requested_to_position;
  uint32_t requested_duration_ms;

  // Carga / guardado
  void load_defaults();
  bool load_from_nvs();
  void save_all();

  // Procesamiento de comunicacion
  void process_json(JsonDocument& doc);
  void set_pending_request(DoorHostRequest req, uint8_t pos);
  void set_pending_led_sim_request(uint8_t fromPos, uint8_t toPos, uint32_t durationMs);

  // JSON responses
  void send_all_params();
  void send_version();
  void send_ok(JsonDocument& doc);
  void send_ack(JsonDocument& doc);
  void send_error(const char* reason);
  void send_json(JsonDocument& doc);
  void send_json_pretty(JsonDocument& doc);

  // Utilidades
  void discard_serial_line();
  void discard_serial_whitespace();
  bool is_serial_whitespace(int c) const;

  float normalize_deg(float deg) const;
  bool valid_float(float value) const;
  uint32_t clamp_pwm(uint32_t value) const;
  uint32_t sanitize_motion_mode(uint32_t value) const;
  uint32_t sanitize_bool01(uint32_t value) const;
  uint32_t sanitize_led_count(uint32_t value) const;
  uint32_t sanitize_led_brightness(uint32_t value) const;
  uint32_t sanitize_led_interval(uint32_t value, uint32_t defaultValue) const;
};

#endif // DOOR_CONFIG_H
