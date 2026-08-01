#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include "timer.h"

/*
  ============================================================
  CButton
  ------------------------------------------------------------
  Responsabilidad:
    - Lectura de un pulsador digital.
    - Generacion de un evento simple de pulsacion.
    - Antirebote no bloqueante mediante CTimer.

  Uso previsto:
    CButton Button;
    Button.init(pin, HIGH, INPUT_PULLUP);
    Button.debounce();
    if (Button.is_pressed()) { ... }

  El objeto se crea en motorized_door.ino. La clase no conoce
  estados de producto ni ejecuta acciones sobre motor o LED.
  ============================================================
*/

#define BUTTON_DEBOUNCE_MS 500UL

class CButton {
public:
  CButton();

  void init(uint8_t pin, uint8_t activeLevel, uint8_t inputMode);
  void debounce();
  bool is_pressed();
  bool is_active() const;

private:
  uint8_t inputPin;
  uint8_t pressedLevel;
  bool initialized;
  bool state;
  bool pressLatched;
  bool releaseTiming;
  CTimer debounceTimer;
};

#endif // BUTTON_H
