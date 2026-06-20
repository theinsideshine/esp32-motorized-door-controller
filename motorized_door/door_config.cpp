#include "door_config.h"
#include <string.h>

CDoorConfig::CDoorConfig()
{
  app_version = "unknown";
  nvs_ready = false;

  pending_request = DOOR_REQ_NONE;
  requested_position = 0;

  load_defaults();
}

void CDoorConfig::set_app_version(const char* version)
{
  if (version == nullptr) {
    app_version = "unknown";
    return;
  }

  app_version = version;
}

bool CDoorConfig::init()
{
  load_defaults();

  nvs_ready = prefs.begin(DOOR_CFG_NAMESPACE, false);

  if (!nvs_ready) {
    return false;
  }

  uint32_t magic = prefs.getUInt("magic", 0);
  uint32_t schema = prefs.getUInt("schema", 0);

  if (magic != DOOR_CFG_MAGIC_NUMBER || schema != DOOR_CFG_SCHEMA_VERSION) {
    load_defaults();
    save_all();
  } else {
    load_from_nvs();
  }

  pending_request = DOOR_REQ_NONE;
  requested_position = 0;

  return true;
}

// ============================================================
// DEFAULTS / NVS
// ============================================================

void CDoorConfig::load_defaults()
{
  pos1_deg = DOOR_POS_1_DEFAULT_DEG;
  pos2_deg = DOOR_POS_2_DEFAULT_DEG;
  pos3_deg = DOOR_POS_3_DEFAULT_DEG;

  pwm_move = DOOR_PWM_MOVE_DEFAULT;

  control_period_us = DOOR_CONTROL_PERIOD_US_DEFAULT;

  auto_tolerance_deg = DOOR_AUTO_TOLERANCE_DEG_DEFAULT;
  auto_cross_margin_deg = DOOR_AUTO_CROSS_MARGIN_DEG_DEFAULT;
  auto_max_run_ms = DOOR_AUTO_MAX_RUN_MS_DEFAULT;
  auto_stall_check_ms = DOOR_AUTO_STALL_CHECK_MS_DEFAULT;
  auto_min_move_deg = DOOR_AUTO_MIN_MOVE_DEG_DEFAULT;
  auto_stall_max_count = DOOR_AUTO_STALL_MAX_COUNT_DEFAULT;
  auto_final_settle_ms = DOOR_AUTO_FINAL_SETTLE_MS_DEFAULT;

  log_level = DOOR_LOG_LEVEL_DEFAULT;
  st_mode = DOOR_ST_MODE_NORMAL;
}

bool CDoorConfig::load_from_nvs()
{
  if (!nvs_ready) {
    return false;
  }

  pos1_deg = normalize_deg(prefs.getFloat("pos1", DOOR_POS_1_DEFAULT_DEG));
  pos2_deg = normalize_deg(prefs.getFloat("pos2", DOOR_POS_2_DEFAULT_DEG));
  pos3_deg = normalize_deg(prefs.getFloat("pos3", DOOR_POS_3_DEFAULT_DEG));

  pwm_move = clamp_pwm(prefs.getUInt("pwm", DOOR_PWM_MOVE_DEFAULT));

  control_period_us = prefs.getUInt("period", DOOR_CONTROL_PERIOD_US_DEFAULT);

  auto_tolerance_deg = prefs.getFloat("tol", DOOR_AUTO_TOLERANCE_DEG_DEFAULT);
  auto_cross_margin_deg = prefs.getFloat("cross", DOOR_AUTO_CROSS_MARGIN_DEG_DEFAULT);
  auto_max_run_ms = prefs.getUInt("maxrun", DOOR_AUTO_MAX_RUN_MS_DEFAULT);
  auto_stall_check_ms = prefs.getUInt("stallms", DOOR_AUTO_STALL_CHECK_MS_DEFAULT);
  auto_min_move_deg = prefs.getFloat("minmov", DOOR_AUTO_MIN_MOVE_DEG_DEFAULT);
  auto_stall_max_count = prefs.getUInt("stallcnt", DOOR_AUTO_STALL_MAX_COUNT_DEFAULT);
  auto_final_settle_ms = prefs.getUInt("settle", DOOR_AUTO_FINAL_SETTLE_MS_DEFAULT);

  log_level = prefs.getUInt("log", DOOR_LOG_LEVEL_DEFAULT);
  st_mode = prefs.getUInt("mode", DOOR_ST_MODE_NORMAL);

  if (!valid_float(pos1_deg)) {
    pos1_deg = DOOR_POS_1_DEFAULT_DEG;
  }

  if (!valid_float(pos2_deg)) {
    pos2_deg = DOOR_POS_2_DEFAULT_DEG;
  }

  if (!valid_float(pos3_deg)) {
    pos3_deg = DOOR_POS_3_DEFAULT_DEG;
  }

  if (!valid_float(auto_tolerance_deg) || auto_tolerance_deg <= 0.0f) {
    auto_tolerance_deg = DOOR_AUTO_TOLERANCE_DEG_DEFAULT;
  }

  if (!valid_float(auto_cross_margin_deg) || auto_cross_margin_deg < 0.0f) {
    auto_cross_margin_deg = DOOR_AUTO_CROSS_MARGIN_DEG_DEFAULT;
  }

  if (!valid_float(auto_min_move_deg) || auto_min_move_deg < 0.0f) {
    auto_min_move_deg = DOOR_AUTO_MIN_MOVE_DEG_DEFAULT;
  }

  if (control_period_us == 0) {
    control_period_us = DOOR_CONTROL_PERIOD_US_DEFAULT;
  }

  if (auto_max_run_ms == 0) {
    auto_max_run_ms = DOOR_AUTO_MAX_RUN_MS_DEFAULT;
  }

  if (auto_stall_check_ms == 0) {
    auto_stall_check_ms = DOOR_AUTO_STALL_CHECK_MS_DEFAULT;
  }

  if (auto_stall_max_count == 0) {
    auto_stall_max_count = DOOR_AUTO_STALL_MAX_COUNT_DEFAULT;
  }

  return true;
}

void CDoorConfig::save_all()
{
  if (!nvs_ready) {
    return;
  }

  prefs.putUInt("magic", DOOR_CFG_MAGIC_NUMBER);
  prefs.putUInt("schema", DOOR_CFG_SCHEMA_VERSION);

  prefs.putFloat("pos1", pos1_deg);
  prefs.putFloat("pos2", pos2_deg);
  prefs.putFloat("pos3", pos3_deg);

  prefs.putUInt("pwm", pwm_move);

  prefs.putUInt("period", control_period_us);

  prefs.putFloat("tol", auto_tolerance_deg);
  prefs.putFloat("cross", auto_cross_margin_deg);
  prefs.putUInt("maxrun", auto_max_run_ms);
  prefs.putUInt("stallms", auto_stall_check_ms);
  prefs.putFloat("minmov", auto_min_move_deg);
  prefs.putUInt("stallcnt", auto_stall_max_count);
  prefs.putUInt("settle", auto_final_settle_ms);

  prefs.putUInt("log", log_level);
  prefs.putUInt("mode", st_mode);
}

// ============================================================
// GETTERS
// ============================================================

float CDoorConfig::get_pos1_deg() const
{
  return pos1_deg;
}

float CDoorConfig::get_pos2_deg() const
{
  return pos2_deg;
}

float CDoorConfig::get_pos3_deg() const
{
  return pos3_deg;
}

float CDoorConfig::get_pos_deg(uint8_t pos) const
{
  switch (pos) {
    case 1:
      return pos1_deg;

    case 2:
      return pos2_deg;

    case 3:
      return pos3_deg;

    default:
      return pos1_deg;
  }
}

uint32_t CDoorConfig::get_pwm_move() const
{
  return pwm_move;
}

uint32_t CDoorConfig::get_control_period_us() const
{
  return control_period_us;
}

float CDoorConfig::get_auto_tolerance_deg() const
{
  return auto_tolerance_deg;
}

float CDoorConfig::get_auto_cross_margin_deg() const
{
  return auto_cross_margin_deg;
}

uint32_t CDoorConfig::get_auto_max_run_ms() const
{
  return auto_max_run_ms;
}

uint32_t CDoorConfig::get_auto_stall_check_ms() const
{
  return auto_stall_check_ms;
}

float CDoorConfig::get_auto_min_move_deg() const
{
  return auto_min_move_deg;
}

uint32_t CDoorConfig::get_auto_stall_max_count() const
{
  return auto_stall_max_count;
}

uint32_t CDoorConfig::get_auto_final_settle_ms() const
{
  return auto_final_settle_ms;
}

uint32_t CDoorConfig::get_log_level() const
{
  return log_level;
}

uint32_t CDoorConfig::get_st_mode() const
{
  return st_mode;
}

// ============================================================
// SETTERS RAM + NVS
// ============================================================

void CDoorConfig::set_pos1_deg(float value)
{
  if (!valid_float(value)) {
    return;
  }

  pos1_deg = normalize_deg(value);

  if (nvs_ready) {
    prefs.putFloat("pos1", pos1_deg);
  }
}

void CDoorConfig::set_pos2_deg(float value)
{
  if (!valid_float(value)) {
    return;
  }

  pos2_deg = normalize_deg(value);

  if (nvs_ready) {
    prefs.putFloat("pos2", pos2_deg);
  }
}

void CDoorConfig::set_pos3_deg(float value)
{
  if (!valid_float(value)) {
    return;
  }

  pos3_deg = normalize_deg(value);

  if (nvs_ready) {
    prefs.putFloat("pos3", pos3_deg);
  }
}

void CDoorConfig::set_pwm_move(uint32_t value)
{
  pwm_move = clamp_pwm(value);

  if (nvs_ready) {
    prefs.putUInt("pwm", pwm_move);
  }
}

void CDoorConfig::set_control_period_us(uint32_t value)
{
  if (value == 0) {
    return;
  }

  control_period_us = value;

  if (nvs_ready) {
    prefs.putUInt("period", control_period_us);
  }
}

void CDoorConfig::set_auto_tolerance_deg(float value)
{
  if (!valid_float(value) || value <= 0.0f) {
    return;
  }

  auto_tolerance_deg = value;

  if (nvs_ready) {
    prefs.putFloat("tol", auto_tolerance_deg);
  }
}

void CDoorConfig::set_auto_cross_margin_deg(float value)
{
  if (!valid_float(value) || value < 0.0f) {
    return;
  }

  auto_cross_margin_deg = value;

  if (nvs_ready) {
    prefs.putFloat("cross", auto_cross_margin_deg);
  }
}

void CDoorConfig::set_auto_max_run_ms(uint32_t value)
{
  if (value == 0) {
    return;
  }

  auto_max_run_ms = value;

  if (nvs_ready) {
    prefs.putUInt("maxrun", auto_max_run_ms);
  }
}

void CDoorConfig::set_auto_stall_check_ms(uint32_t value)
{
  if (value == 0) {
    return;
  }

  auto_stall_check_ms = value;

  if (nvs_ready) {
    prefs.putUInt("stallms", auto_stall_check_ms);
  }
}

void CDoorConfig::set_auto_min_move_deg(float value)
{
  if (!valid_float(value) || value < 0.0f) {
    return;
  }

  auto_min_move_deg = value;

  if (nvs_ready) {
    prefs.putFloat("minmov", auto_min_move_deg);
  }
}

void CDoorConfig::set_auto_stall_max_count(uint32_t value)
{
  if (value == 0) {
    return;
  }

  auto_stall_max_count = value;

  if (nvs_ready) {
    prefs.putUInt("stallcnt", auto_stall_max_count);
  }
}

void CDoorConfig::set_auto_final_settle_ms(uint32_t value)
{
  auto_final_settle_ms = value;

  if (nvs_ready) {
    prefs.putUInt("settle", auto_final_settle_ms);
  }
}

void CDoorConfig::set_log_level(uint32_t value)
{
  log_level = value;

  if (nvs_ready) {
    prefs.putUInt("log", log_level);
  }
}

void CDoorConfig::set_st_mode(uint32_t value)
{
  st_mode = value;

  if (nvs_ready) {
    prefs.putUInt("mode", st_mode);
  }
}

// ============================================================
// PEDIDOS PENDIENTES
// ============================================================

bool CDoorConfig::has_request() const
{
  return pending_request != DOOR_REQ_NONE;
}

DoorHostRequest CDoorConfig::get_request() const
{
  return pending_request;
}

uint8_t CDoorConfig::get_requested_position() const
{
  return requested_position;
}

void CDoorConfig::clear_request()
{
  pending_request = DOOR_REQ_NONE;
  requested_position = 0;
}

void CDoorConfig::set_pending_request(DoorHostRequest req, uint8_t pos)
{
  pending_request = req;
  requested_position = pos;
}

// ============================================================
// HOST CMD - SOLO JSON
// ============================================================

void CDoorConfig::host_cmd()
{
  if (Serial.available() <= 0) {
    return;
  }

  // No se aceptan comandos por caracter. Solo se descarta whitespace
  // residual de Serial Monitor para no responder errores por \r/\n.
  discard_serial_whitespace();

  if (Serial.available() <= 0) {
    return;
  }

  int first = Serial.peek();

  if (first != '{') {
    discard_serial_line();
    send_error("json_expected");
    return;
  }

  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, Serial);

  if (error) {
    discard_serial_line();

    if (error == DeserializationError::EmptyInput) {
      return;
    }

    send_error("json_parse_error");
    return;
  }

  discard_serial_whitespace();

  process_json(doc);
}

void CDoorConfig::process_json(JsonDocument& doc)
{
  if (!doc.is<JsonObject>()) {
    send_error("json_object_expected");
    return;
  }

  bool known_key = false;

  // ----------------------------------------------------------
  // Config persistente
  // ----------------------------------------------------------

  if (doc.containsKey("pos1_deg")) {
    set_pos1_deg(doc["pos1_deg"].as<float>());
    doc["pos1_deg"] = pos1_deg;
    known_key = true;
  }

  if (doc.containsKey("pos2_deg")) {
    set_pos2_deg(doc["pos2_deg"].as<float>());
    doc["pos2_deg"] = pos2_deg;
    known_key = true;
  }

  if (doc.containsKey("pos3_deg")) {
    set_pos3_deg(doc["pos3_deg"].as<float>());
    doc["pos3_deg"] = pos3_deg;
    known_key = true;
  }

  if (doc.containsKey("pwm_move")) {
    set_pwm_move(doc["pwm_move"].as<uint32_t>());
    doc["pwm_move"] = pwm_move;
    known_key = true;
  }

  if (doc.containsKey("control_period_us")) {
    set_control_period_us(doc["control_period_us"].as<uint32_t>());
    doc["control_period_us"] = control_period_us;
    known_key = true;
  }

  if (doc.containsKey("auto_tolerance_deg")) {
    set_auto_tolerance_deg(doc["auto_tolerance_deg"].as<float>());
    doc["auto_tolerance_deg"] = auto_tolerance_deg;
    known_key = true;
  }

  if (doc.containsKey("auto_cross_margin_deg")) {
    set_auto_cross_margin_deg(doc["auto_cross_margin_deg"].as<float>());
    doc["auto_cross_margin_deg"] = auto_cross_margin_deg;
    known_key = true;
  }

  if (doc.containsKey("auto_max_run_ms")) {
    set_auto_max_run_ms(doc["auto_max_run_ms"].as<uint32_t>());
    doc["auto_max_run_ms"] = auto_max_run_ms;
    known_key = true;
  }

  if (doc.containsKey("auto_stall_check_ms")) {
    set_auto_stall_check_ms(doc["auto_stall_check_ms"].as<uint32_t>());
    doc["auto_stall_check_ms"] = auto_stall_check_ms;
    known_key = true;
  }

  if (doc.containsKey("auto_min_move_deg")) {
    set_auto_min_move_deg(doc["auto_min_move_deg"].as<float>());
    doc["auto_min_move_deg"] = auto_min_move_deg;
    known_key = true;
  }

  if (doc.containsKey("auto_stall_max_count")) {
    set_auto_stall_max_count(doc["auto_stall_max_count"].as<uint32_t>());
    doc["auto_stall_max_count"] = auto_stall_max_count;
    known_key = true;
  }

  if (doc.containsKey("auto_final_settle_ms")) {
    set_auto_final_settle_ms(doc["auto_final_settle_ms"].as<uint32_t>());
    doc["auto_final_settle_ms"] = auto_final_settle_ms;
    known_key = true;
  }

  if (doc.containsKey("log_level")) {
    set_log_level(doc["log_level"].as<uint32_t>());
    doc["log_level"] = log_level;
    known_key = true;
  }

  if (doc.containsKey("st_mode")) {
    set_st_mode(doc["st_mode"].as<uint32_t>());
    doc["st_mode"] = st_mode;
    known_key = true;
  }

  // ----------------------------------------------------------
  // Info
  // ----------------------------------------------------------

  if (doc.containsKey("info")) {
    const char* key = doc["info"] | "";

    if (strcmp(key, "all-params") == 0) {
      send_all_params();
      return;
    }

    if (strcmp(key, "version") == 0) {
      send_version();
      return;
    }

    send_error("unknown_info");
    return;
  }

  // ----------------------------------------------------------
  // Comandos runtime: dejan pedido pendiente para el main
  // ----------------------------------------------------------

  if (doc.containsKey("cmd")) {
    const char* key = doc["cmd"] | "";

    if (strcmp(key, "go") == 0) {
      uint8_t pos = doc["pos"] | 0;

      if (pos == 1) {
        set_pending_request(DOOR_REQ_GO_POS_1, 1);
        send_ack(doc);
        return;
      }

      if (pos == 2) {
        set_pending_request(DOOR_REQ_GO_POS_2, 2);
        send_ack(doc);
        return;
      }

      if (pos == 3) {
        set_pending_request(DOOR_REQ_GO_POS_3, 3);
        send_ack(doc);
        return;
      }

      send_error("invalid_pos");
      return;
    }

    if (strcmp(key, "stop") == 0) {
      set_pending_request(DOOR_REQ_STOP, 0);
      send_ack(doc);
      return;
    }

    if (strcmp(key, "factory-reset") == 0) {
      factory_reset();
      send_ok(doc);
      return;
    }

    send_error("unknown_cmd");
    return;
  }

  if (known_key) {
    send_ok(doc);
    return;
  }

  send_error("unknown_key");
}

// ============================================================
// JSON RESPONSES
// ============================================================

void CDoorConfig::send_all_params()
{
  StaticJsonDocument<768> doc;

  doc["info"] = "all-params";
  doc["result"] = "ok";
  doc["app_version"] = app_version;

  doc["magic"] = DOOR_CFG_MAGIC_NUMBER;
  doc["schema"] = DOOR_CFG_SCHEMA_VERSION;

  doc["pos1_deg"] = pos1_deg;
  doc["pos2_deg"] = pos2_deg;
  doc["pos3_deg"] = pos3_deg;

  doc["pwm_move"] = pwm_move;

  doc["control_period_us"] = control_period_us;

  doc["auto_tolerance_deg"] = auto_tolerance_deg;
  doc["auto_cross_margin_deg"] = auto_cross_margin_deg;
  doc["auto_max_run_ms"] = auto_max_run_ms;
  doc["auto_stall_check_ms"] = auto_stall_check_ms;
  doc["auto_min_move_deg"] = auto_min_move_deg;
  doc["auto_stall_max_count"] = auto_stall_max_count;
  doc["auto_final_settle_ms"] = auto_final_settle_ms;

  doc["log_level"] = log_level;
  doc["st_mode"] = st_mode;

  doc["pending_request"] = (uint8_t)pending_request;
  doc["requested_position"] = requested_position;

  send_json_pretty(doc);
}

void CDoorConfig::send_version()
{
  StaticJsonDocument<256> doc;

  doc["info"] = "version";
  doc["result"] = "ok";
  doc["app_version"] = app_version;

  send_json(doc);
}

void CDoorConfig::send_ok(JsonDocument& doc)
{
  doc["result"] = "ok";

  send_json(doc);
}

void CDoorConfig::send_ack(JsonDocument& doc)
{
  doc["result"] = "ack";

  send_json(doc);
}

void CDoorConfig::send_error(const char* reason)
{
  StaticJsonDocument<192> doc;

  doc["result"] = "error";
  doc["reason"] = reason;

  send_json(doc);
}

void CDoorConfig::send_json(JsonDocument& doc)
{
  serializeJson(doc, Serial);
  Serial.println();
  Serial.println();
}

void CDoorConfig::send_json_pretty(JsonDocument& doc)
{
  serializeJsonPretty(doc, Serial);
  Serial.println();
  Serial.println();
}

void CDoorConfig::factory_reset()
{
  load_defaults();
  save_all();

  pending_request = DOOR_REQ_NONE;
  requested_position = 0;
}

// ============================================================
// UTILIDADES SERIAL
// ============================================================

void CDoorConfig::discard_serial_line()
{
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n') {
      break;
    }
  }
}

void CDoorConfig::discard_serial_whitespace()
{
  while (Serial.available() > 0) {
    int c = Serial.peek();

    if (!is_serial_whitespace(c)) {
      return;
    }

    Serial.read();
  }
}

bool CDoorConfig::is_serial_whitespace(int c) const
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// ============================================================
// UTILIDADES
// ============================================================

float CDoorConfig::normalize_deg(float deg) const
{
  if (!valid_float(deg)) {
    return 0.0f;
  }

  while (deg < 0.0f) {
    deg += 360.0f;
  }

  while (deg >= 360.0f) {
    deg -= 360.0f;
  }

  return deg;
}

bool CDoorConfig::valid_float(float value) const
{
  return !isnan(value) && value > -1000000.0f && value < 1000000.0f;
}

uint32_t CDoorConfig::clamp_pwm(uint32_t value) const
{
  if (value > 255UL) {
    return 255UL;
  }

  return value;
}
