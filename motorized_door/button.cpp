#include "button.h"

CButton::CButton()
{
  inputPin = 0;
  pressedLevel = LOW;
  initialized = false;
  state = false;
  pressLatched = false;
  releaseTiming = false;
}

void CButton::init(uint8_t pin, uint8_t activeLevel, uint8_t inputMode)
{
  inputPin = pin;
  pressedLevel = activeLevel;
  state = false;
  pressLatched = false;
  releaseTiming = false;

  pinMode(inputPin, inputMode);
  debounceTimer.start();
  initialized = true;
}

// Devuelve el evento una sola vez y limpia el estado interno.
bool CButton::is_pressed()
{
  bool retVal = state;
  state = false;
  return retVal;
}

bool CButton::is_active() const
{
  if (!initialized) {
    return false;
  }

  return digitalRead(inputPin) == pressedLevel;
}

// Patron simple basado en el proyecto Cartel, usando CTimer.
// Una pulsacion genera un unico evento y exige 500 ms liberado
// antes de aceptar una nueva pulsacion.
void CButton::debounce()
{
  state = false;

  if (!initialized) {
    return;
  }

  if (is_active()) {
    releaseTiming = false;

    if (!pressLatched && debounceTimer.expired_ms(BUTTON_DEBOUNCE_MS)) {
      state = true;
      pressLatched = true;
    }

    return;
  }

  if (!pressLatched) {
    return;
  }

  if (!releaseTiming) {
    debounceTimer.start();
    releaseTiming = true;
    return;
  }

  if (debounceTimer.expired_ms(BUTTON_DEBOUNCE_MS)) {
    pressLatched = false;
    releaseTiming = false;
  }
}
