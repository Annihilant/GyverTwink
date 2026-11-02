//Timer.h
#pragma once

#include <Arduino.h>

struct Timer {
  Timer(uint32_t prd = 0, boolean state = true) {
    setPrd(prd);
    _state = state;
    // _tmr будет установлен при первом restart() или в ready()
  }

  void setPrd(uint32_t prd) {
    if (prd > 0) {
      _prd = prd;
    } else {
      stop();
    }
  }

  bool ready() {
    uint32_t now = millis();  // читаем один раз — защита от скачка millis()
    if (_state && (now - _tmr) >= _prd) {
      restart();
      return true;
    }
    return false;
  }

  void restart() {
    _tmr = millis();
    _state = true;
  }

  void stop() {
    _state = false;
  }

  bool state() const {
    return _state;
  }

private:
  uint32_t _tmr = 0;
  uint32_t _prd = 0;
  bool _state = true;
};
