#include "timer.h"

CTimer::CTimer()
{
  start();
}

void CTimer::start(void)
{
  timerMs = millis();
  timerUs = micros();
}

void CTimer::start_ms_ago(uint32_t ms)
{
  timerMs = millis() - ms;
  timerUs = micros();
}

void CTimer::start_us_ago(uint32_t us)
{
  timerMs = millis();
  timerUs = micros() - us;
}

bool CTimer::expired(uint32_t ms) const
{
  return expired_ms(ms);
}

bool CTimer::expired_ms(uint32_t ms) const
{
  return ((uint32_t)(millis() - timerMs) >= ms);
}

bool CTimer::expired_us(uint32_t us) const
{
  return ((uint32_t)(micros() - timerUs) >= us);
}

uint32_t CTimer::elapsed_ms(void) const
{
  return (uint32_t)(millis() - timerMs);
}

uint32_t CTimer::elapsed_us(void) const
{
  return (uint32_t)(micros() - timerUs);
}