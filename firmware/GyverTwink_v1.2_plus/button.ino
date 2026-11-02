//button.ino
void button() {
  static bool dir = 1;

  btn.tick();

  if (btn.hasClicks(1)) {
    DEBUGLN("1x click - power");
    cfg.power = !cfg.power;
    if (!cfg.power) {
      stopFader();            // НОВОЕ: гасим и спецэффект, иначе он "оживит" ленту
      strip->showLeds(0);
    }
    EEcfg.update();
  }

  if (btn.hasClicks(2)) {
    DEBUGLN("2x click - next");
    switchEff();
  }

  // НОВОЕ: тройное нажатие — запуск/остановка мигания
  if (btn.hasClicks(3)) {
    if (faderState.active) stopFader();          // мигает → выключить
    else startFadeBlink(CRGB::Red);              // не мигает → включить
  }

  if (btn.step()) {
    DEBUGLN("change bright");
    if (cfg.power) {
      int bri = cfg.bright;
      bri += dir ? 30 : -30;
      bri = constrain(bri, 0, 255);
      cfg.bright = bri;
      if (bri == 255) {
        strip->showLeds(0);
        delay(100);            // оставил: разовая короткая индикация, безопасно
        strip->showLeds(bri);
      }
      EEcfg.update();
    }
  }

  if (btn.releaseStep()) {
    DEBUGLN("change dir");
    dir = !dir;
  }
}
