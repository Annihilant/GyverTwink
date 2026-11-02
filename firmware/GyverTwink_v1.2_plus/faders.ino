//  faders.ino
//  1) Блокирующие функции — ТОЛЬКО для startup.ino (setup),
//     там блокировка безопасна (Wi-Fi ещё не работает)
//  2) Неблокирующие — для loop(): кнопка, UDP-команды
// ============================================================

#include <FastLED.h>

// ---------- 1. СТАРЫЕ блокирующие (для startup.ino) ----------

void startupFader(CRGB color) {
  static uint32_t tmr;
  static int val = 0;
  static bool dir = true;
  if (millis() - tmr >= 20) {
    tmr = millis();
    val += dir ? 3 : -3;
    val = constrain(val, 5, 120);
    if (val >= 120 || val <= 5) dir = !dir;
    strip->showColor(color, val);
  }
}

void startupFadeBlink(CRGB color) {
  for (int i = 0; i < 200; i += 20) {
    strip->showColor(color, i);
    delay(20);
  }
  for (int i = 200; i > 0; i -= 20) {
    strip->showColor(color, i);
    delay(20);
  }
  strip->clearLedData();
  strip->showLeds(0);
}

// ---------- 2. НОВЫЕ неблокирующие (для loop) ----------

// Один шаг фейдера. Вызывается из loop(), пока faderState.active
void stepFader() {
  if (!faderState.active) return;

  uint32_t now = millis();
  if (now - faderState.lastTime < 20) return;  // шаг раз в 20 мс
  faderState.lastTime = now;

  faderState.val += faderState.dir ? faderState.step : -faderState.step;

  if (faderState.val >= 200) {
    faderState.val = 200;
    faderState.dir = false;
  } else if (faderState.val <= 0) {
    faderState.val = 0;
    if (faderState.mode == 1) {
      faderState.dir = true;   // blink: разворачиваемся
    } else {
      faderState.active = false;  // fade: эффект окончен
      fill_solid(leds, cfg.ledAm, CRGB::Black);
      strip->showLeds(0);
      return;
    }
  }

  fill_solid(leds, cfg.ledAm, faderState.color);
  strip->showLeds(faderState.val);  // showLeds сам масштабирует яркость
}

// Однократный плавный подъём яркости 0 -> target
void startFade(CRGB color, int target) {
  faderState.color = color;
  faderState.val = 0;
  faderState.dir = true;
  faderState.mode = 0;
  faderState.active = true;
  faderState.lastTime = millis();
  (void)target;
}

// Мигание туда-обратно, работает пока не вызвать stopFader()
void startFadeBlink(CRGB color) {
  faderState.color = color;
  faderState.val = 0;
  faderState.dir = true;
  faderState.mode = 1;
  faderState.active = true;
  faderState.lastTime = millis();
}
