#ifndef DOOR_LED_STRIP_H
#define DOOR_LED_STRIP_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "timer.h"

/*
  ============================================================
  CLedStrip
  ------------------------------------------------------------
  Responsabilidad:
    - Maquina de estados independiente para tira WS2812B.
    - Animaciones no bloqueantes basadas en CTimer.
    - No procesa JSON, no guarda NVS, no conoce DoorMotion.

  Estados:
    - OFF: apagado.
    - IDLE: azul respiracion suave.
    - MOVING_FWD: verde desplazandose hacia adelante logico.
    - MOVING_RWD: verde desplazandose hacia atras logico.
    - OPEN_WAIT: verde fijo durante la espera con puerta abierta.
    - CLOSING_FWD/RWD: rojo desplazandose durante el cierre.
    - ARRIVED: pausa breve conservando ultimo cuadro antes de volver a IDLE.
    - ALARM: rojo intermitente.
    - ERROR: rojo/amarillo rapido.
  ============================================================
*/

enum LedStripState : uint8_t {
  LED_STRIP_OFF = 0,
  LED_STRIP_IDLE,
  LED_STRIP_MOVING_FWD,
  LED_STRIP_MOVING_RWD,
  LED_STRIP_OPEN_WAIT,
  LED_STRIP_CLOSING_FWD,
  LED_STRIP_CLOSING_RWD,
  LED_STRIP_ARRIVED,
  LED_STRIP_ALARM,
  LED_STRIP_ERROR
};

class CLedStrip {
public:
  CLedStrip();

  void begin(uint8_t dataPin, uint16_t ledCount, uint8_t brightness);

  void configure(bool enabled,
                 uint16_t ledCount,
                 uint8_t brightness,
                 uint32_t stepMs,
                 uint32_t breathMs,
                 uint32_t blinkMs);

  void set_state(LedStripState newState);
  LedStripState state() const;
  const char* state_name() const;

  void update();

private:
  Adafruit_NeoPixel pixels;

  bool initialized;
  bool enabled;
  bool forceRender;

  uint8_t pinData;
  uint16_t count;
  uint8_t globalBrightness;

  uint32_t stepPeriodMs;
  uint32_t breathPeriodMs;
  uint32_t blinkPeriodMs;

  LedStripState currentState;

  CTimer animationTimer;

  uint32_t arrivedHoldMs;

  uint16_t movingIndex;
  int16_t breathLevel;
  int8_t breathDirection;
  bool blinkOn;
  bool errorPhase;

  uint32_t make_color(uint8_t r, uint8_t g, uint8_t b);

  void clear_pixels();
  void show_off();

  void reset_animation_for_state(LedStripState newState);

  void render_idle();
  void render_moving(bool forward);
  void render_open_wait();
  void render_closing(bool forward);
  void render_arrived();
  void render_alarm();
  void render_error();
};

#endif // DOOR_LED_STRIP_H
