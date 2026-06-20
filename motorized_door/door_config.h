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
// DEFAULTS EQUIVALENTES A v3.1 VALIDADA
// ============================================================

#define DOOR_POS_1_DEFAULT_DEG    2.29f
#define DOOR_POS_2_DEFAULT_DEG    291.23f
#define DOOR_POS_3_DEFAULT_DEG    206.06f

#define DOOR_PWM_MOVE_DEFAULT     70UL

#define DOOR_CONTROL_PERIOD_US_DEFAULT        5000UL
#define DOOR_AUTO_TOLERANCE_DEG_DEFAULT       1.50f
#define DOOR_AUTO_CROSS_MARGIN_DEG_DEFAULT    0.20f
#define DOOR_AUTO_MAX_RUN_MS_DEFAULT          10000UL
#define DOOR_AUTO_STALL_CHECK_MS_DEFAULT      250UL
#define DOOR_AUTO_MIN_MOVE_DEG_DEFAULT        0.30f
#define DOOR_AUTO_STALL_MAX_COUNT_DEFAULT     3UL
#define DOOR_AUTO_FINAL_SETTLE_MS_DEFAULT     80UL

#define DOOR_LOG_LEVEL_DEFAULT                1UL

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
  DOOR_REQ_STOP
};

class CDoorConfig {
public:
  CDoorConfig();

  bool init();

  /*
    Lee comunicacion por Serial.

    Protocolo host: SOLO JSON.

    Ejemplos:
      {"info":"all-params"}
      {"cmd":"go","pos":2}
      {"cmd":"stop"}
      {"cmd":"factory-reset"}
      {"pwm_move":70}
      {"pos1_deg":2.29}
      {"pos2_deg":291.23}
      {"pos3_deg":206.06}
      {"log_level":1}
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

  // ==========================================================
  // Setters: actualizan RAM + NVS
  // ==========================================================

  void set_pos1_deg(float value);
  void set_pos2_deg(float value);
  void set_pos3_deg(float value);

  void set_pwm_move(uint32_t value);

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

  // ==========================================================
  // Pedidos pendientes para que el main decida
  // ==========================================================

  bool has_request() const;
  DoorHostRequest get_request() const;
  uint8_t get_requested_position() const;
  void clear_request();

  // ==========================================================
  // Info / reset
  // ==========================================================

  void factory_reset();

private:
  Preferences prefs;

  bool nvs_ready;

  // Copia RAM persistente
  float pos1_deg;
  float pos2_deg;
  float pos3_deg;

  uint32_t pwm_move;

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

  // Runtime / comunicacion
  DoorHostRequest pending_request;
  uint8_t requested_position;

  // Carga / guardado
  void load_defaults();
  bool load_from_nvs();
  void save_all();

  // Procesamiento de comunicacion
  void process_json(JsonDocument& doc);
  void set_pending_request(DoorHostRequest req, uint8_t pos);

  // JSON responses
  void send_all_params();
  void send_ok(JsonDocument& doc);
  void send_ack(JsonDocument& doc);
  void send_error(const char* reason);
  void send_json(JsonDocument& doc);

  // Utilidades
  void discard_serial_line();
  void discard_serial_whitespace();
  bool is_serial_whitespace(int c) const;

  float normalize_deg(float deg) const;
  bool valid_float(float value) const;
  uint32_t clamp_pwm(uint32_t value) const;
};

#endif // DOOR_CONFIG_H
