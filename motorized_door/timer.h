#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

/*
  ============================================================
  CTimer
  ------------------------------------------------------------
  Responsabilidad:
    - Timer por software basado en millis() y micros().
    - Encapsula calculo de expiracion y tiempo transcurrido.
    - Soporta rollover por resta unsigned.

  Uso:
    timer.start();
    if (timer.expired_ms(1000)) { ... }
    if (timer.expired_us(5000)) { ... }

  Compatibilidad:
    expired(ms) conserva el uso original basado en millis().
  ============================================================
*/

class CTimer {
public:
  CTimer();

  void start(void);
  void start_ms_ago(uint32_t ms);
  void start_us_ago(uint32_t us);

  bool expired(uint32_t ms) const;
  bool expired_ms(uint32_t ms) const;
  bool expired_us(uint32_t us) const;

  uint32_t elapsed_ms(void) const;
  uint32_t elapsed_us(void) const;

private:
  uint32_t timerMs;
  uint32_t timerUs;
};

#endif // TIMER_H
