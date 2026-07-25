#include "door_led_strip.h"

CLedStrip::CLedStrip()
  : pixels(0, 0, NEO_GRB + NEO_KHZ800)
{
  initialized = false;
  enabled = false;
  forceRender = true;

  pinData = 0;
  count = 0;
  globalBrightness = 25;

  stepPeriodMs = 100;
  breathPeriodMs = 25;
  blinkPeriodMs = 180;
  arrivedHoldMs = 140;

  currentState = LED_STRIP_OFF;

  movingIndex = 0;
  breathLevel = 0;
  breathDirection = 1;
  blinkOn = false;
  errorPhase = false;
}

void CLedStrip::begin(uint8_t dataPin, uint16_t ledCount, uint8_t brightness)
{
  pinData = dataPin;
  count = ledCount;
  globalBrightness = brightness;

  pixels.updateType(NEO_GRB + NEO_KHZ800);
  pixels.updateLength(count);
  pixels.setPin(pinData);
  pixels.setBrightness(globalBrightness);
  pixels.begin();

  initialized = true;
  forceRender = true;

  show_off();
}

void CLedStrip::configure(bool newEnabled,
                          uint16_t ledCount,
                          uint8_t brightness,
                          uint32_t stepMs,
                          uint32_t breathMs,
                          uint32_t blinkMs)
{
  if (ledCount == 0) {
    ledCount = 1;
  }

  if (stepMs == 0) {
    stepMs = 100;
  }

  if (breathMs == 0) {
    breathMs = 25;
  }

  if (blinkMs == 0) {
    blinkMs = 180;
  }

  bool changed = enabled != newEnabled ||
                 count != ledCount ||
                 globalBrightness != brightness ||
                 stepPeriodMs != stepMs ||
                 breathPeriodMs != breathMs ||
                 blinkPeriodMs != blinkMs;

  enabled = newEnabled;
  stepPeriodMs = stepMs;
  breathPeriodMs = breathMs;
  blinkPeriodMs = blinkMs;

  if (initialized && count != ledCount) {
    count = ledCount;
    pixels.updateLength(count);
    forceRender = true;
  } else {
    count = ledCount;
  }

  if (initialized && globalBrightness != brightness) {
    globalBrightness = brightness;
    pixels.setBrightness(globalBrightness);
    forceRender = true;
  } else {
    globalBrightness = brightness;
  }

  if (changed) {
    forceRender = true;
  }

  if (!enabled) {
    currentState = LED_STRIP_OFF;
    show_off();
  }
}

void CLedStrip::set_state(LedStripState newState)
{
  if (!enabled) {
    newState = LED_STRIP_OFF;
  }

  if (currentState == newState) {
    return;
  }

  if (currentState == LED_STRIP_ARRIVED && newState == LED_STRIP_IDLE) {
    return;
  }

  if ((currentState == LED_STRIP_MOVING_FWD || currentState == LED_STRIP_MOVING_RWD) &&
      newState == LED_STRIP_IDLE) {
    newState = LED_STRIP_ARRIVED;
  }

  currentState = newState;
  reset_animation_for_state(newState);
  forceRender = true;
}

LedStripState CLedStrip::state() const
{
  return currentState;
}

const char* CLedStrip::state_name() const
{
  switch (currentState) {
    case LED_STRIP_IDLE:
      return "IDLE";

    case LED_STRIP_MOVING_FWD:
      return "MOVING_FWD";

    case LED_STRIP_MOVING_RWD:
      return "MOVING_RWD";

    case LED_STRIP_ARRIVED:
      return "ARRIVED";

    case LED_STRIP_ALARM:
      return "ALARM";

    case LED_STRIP_ERROR:
      return "ERROR";

    case LED_STRIP_OFF:
    default:
      return "OFF";
  }
}

void CLedStrip::update()
{
  if (!initialized) {
    return;
  }

  if (!enabled || currentState == LED_STRIP_OFF) {
    if (forceRender) {
      show_off();
      forceRender = false;
    }

    return;
  }

  switch (currentState) {
    case LED_STRIP_IDLE:
      if (forceRender || animationTimer.expired_ms(breathPeriodMs)) {
        animationTimer.start();
        render_idle();
        forceRender = false;
      }
      break;

    case LED_STRIP_MOVING_FWD:
      if (forceRender || animationTimer.expired_ms(stepPeriodMs)) {
        animationTimer.start();
        render_moving(true);
        forceRender = false;
      }
      break;

    case LED_STRIP_MOVING_RWD:
      if (forceRender || animationTimer.expired_ms(stepPeriodMs)) {
        animationTimer.start();
        render_moving(false);
        forceRender = false;
      }
      break;

    case LED_STRIP_ARRIVED:
      if (forceRender) {
        // No pintar todos los LEDs juntos al llegar.
        // Se mantiene congelado el ultimo cuadro de MOVING para evitar
        // el destello verde simultaneo que se veia como un error visual.
        forceRender = false;
      }

      if (animationTimer.expired_ms(arrivedHoldMs)) {
        currentState = LED_STRIP_IDLE;
        reset_animation_for_state(LED_STRIP_IDLE);
        forceRender = true;
      }
      break;

    case LED_STRIP_ALARM:
      if (forceRender || animationTimer.expired_ms(blinkPeriodMs)) {
        animationTimer.start();
        render_alarm();
        forceRender = false;
      }
      break;

    case LED_STRIP_ERROR: {
      uint32_t errorPeriodMs = blinkPeriodMs / 2;

      if (errorPeriodMs < 60) {
        errorPeriodMs = 60;
      }

      if (forceRender || animationTimer.expired_ms(errorPeriodMs)) {
        animationTimer.start();
        render_error();
        forceRender = false;
      }
      break;
    }

    case LED_STRIP_OFF:
    default:
      if (forceRender) {
        show_off();
        forceRender = false;
      }
      break;
  }
}

uint32_t CLedStrip::make_color(uint8_t r, uint8_t g, uint8_t b)
{
  return pixels.Color(r, g, b);
}

void CLedStrip::clear_pixels()
{
  pixels.clear();
}

void CLedStrip::show_off()
{
  if (!initialized) {
    return;
  }

  clear_pixels();
  pixels.show();
}

void CLedStrip::reset_animation_for_state(LedStripState newState)
{
  animationTimer.start();

  switch (newState) {
    case LED_STRIP_IDLE:
      breathLevel = 40;
      breathDirection = 1;
      break;

    case LED_STRIP_MOVING_FWD:
    case LED_STRIP_MOVING_RWD:
      movingIndex = 0;
      break;

    case LED_STRIP_ARRIVED:
      break;

    case LED_STRIP_ALARM:
      blinkOn = false;
      break;

    case LED_STRIP_ERROR:
      errorPhase = false;
      break;

    case LED_STRIP_OFF:
    default:
      break;
  }
}

void CLedStrip::render_idle()
{
  breathLevel += (int16_t)breathDirection * 5;

  if (breathLevel >= 255) {
    breathLevel = 255;
    breathDirection = -1;
  }

  if (breathLevel <= 0) {
    breathLevel = 0;
    breathDirection = 1;
  }

  uint8_t blue = (uint8_t)breathLevel;

  for (uint16_t i = 0; i < count; i++) {
    pixels.setPixelColor(i, make_color(0, 0, blue));
  }

  pixels.show();
}

void CLedStrip::render_moving(bool forward)
{
  clear_pixels();

  for (uint8_t tail = 0; tail < 3 && tail < count; tail++) {
    uint16_t pos;

    if (forward) {
      pos = (movingIndex + count - tail) % count;
    } else {
      pos = (movingIndex + tail) % count;
    }

    uint8_t green = 0;

    if (tail == 0) {
      green = 255;
    } else if (tail == 1) {
      green = 80;
    } else {
      green = 20;
    }

    pixels.setPixelColor(pos, make_color(0, green, 0));
  }

  pixels.show();

  if (forward) {
    movingIndex = (movingIndex + 1) % count;
  } else {
    if (movingIndex == 0) {
      movingIndex = count - 1;
    } else {
      movingIndex--;
    }
  }
}

void CLedStrip::render_arrived()
{
  // Intencionalmente vacio.
  // ARRIVED conserva el ultimo patron de MOVING por un instante y luego
  // pasa a IDLE. Esto evita un flash de todos los LEDs verdes juntos.
}

void CLedStrip::render_alarm()
{
  blinkOn = !blinkOn;

  if (!blinkOn) {
    show_off();
    return;
  }

  for (uint16_t i = 0; i < count; i++) {
    pixels.setPixelColor(i, make_color(255, 0, 0));
  }

  pixels.show();
}

void CLedStrip::render_error()
{
  errorPhase = !errorPhase;

  uint32_t c;

  if (errorPhase) {
    c = make_color(255, 0, 0);
  } else {
    c = make_color(255, 180, 0);
  }

  for (uint16_t i = 0; i < count; i++) {
    pixels.setPixelColor(i, c);
  }

  pixels.show();
}
