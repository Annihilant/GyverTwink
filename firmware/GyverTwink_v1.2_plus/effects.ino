//effects.ino
void effects() {
  static Timer effTmr(30);
  static uint16_t countP = 0;
  static byte countSkip = 0;
  static byte prevEff = 255;
  static byte fadeCount = 0;
  static byte flyFamInitFor = 255;   // семейство светлячков, для которого инициализирован рой

  if (effTmr.ready()) {
    byte thisEffect;

    if (forceTmr.state()) thisEffect = forceEff;
    else thisEffect = curEff;

    if (prevEff != thisEffect) {
      prevEff = thisEffect;
      fadeCount = 25;
    }

    byte scale = effs[thisEffect].scale;
    byte speed = effs[thisEffect].speed;

// ===== семейство 4 — салют: точка → взрыв кольцом → осыпание =====
    
    if (thisEffect >= ACTIVE_PALETTES * 4) {
      salute(thisEffect, scale, speed);
      return;
    }

// ================================================================

// ================= семейство 3 — звёздное небо ==================

    if (thisEffect >= ACTIVE_PALETTES * 3) {
      byte curPal = thisEffect % ACTIVE_PALETTES;   // палитра из номера эффекта
      static uint16_t stLife[LED_MAX];  // жизнь вспышки в тиках: старт 333 ≈ 10 с
      static byte stHue[LED_MAX];       // свой цвет у каждой звезды
      static byte stPh[LED_MAX];        // фаза пульсации
      static uint16_t spawnAcc = 0;     // накопитель звёзд
      static byte stInitFor = 255;
      static int stLedAm = 0;
      if (stInitFor != thisEffect || stLedAm != cfg.ledAm) {
        memset(stLife, 0, sizeof(stLife));
        for (int i = 0; i < cfg.ledAm; i++) {
          stHue[i] = random8();
          stPh[i] = random8();          // рассинхрон фаз — звёзды не пульсируют в такт
        }
        stInitFor = thisEffect;
        stLedAm = cfg.ledAm;
        spawnAcc = 0;
      }

      // скорость: длительность вспышки 10–1 с (больше ползунок — дольше)
      byte dec = 1 + (uint16_t)speed * 9 / 255;     // шаг угасания 1–10 за тик
      // масштаб: 1–20 звёзд в секунду (больше ползунок — чаще)
      byte spawnStep = 8 + (uint16_t)scale * 146 / 255; // 8–154 из 256 за тик

      // накопитель: копим «долю звезды», набралась целая — зажигаем
      spawnAcc += spawnStep;
      while (spawnAcc >= 256) {
        spawnAcc -= 256;
        byte pos = random(cfg.ledAm);
        if (!stLife[pos]) {          // место свободно — зажигаем
          stLife[pos] = 333;
          stHue[pos] = random8();
        }
        // занято — звезда пропускается, средняя частота чуть падает
      }

      for (int i = 0; i < cfg.ledAm; i++) {
        // фон 5% — не зависит от глобальной яркости (компенсация showLeds)
        CRGB bgc = ColorFromPalette(paletteArr[curPal], 0, 255, LINEARBLEND);
        byte r5 = (uint16_t)bgc.r * 13 / 255;
        byte g5 = (uint16_t)bgc.g * 13 / 255;
        byte b5 = (uint16_t)bgc.b * 13 / 255;
        // тёмные палитры: ненулевые каналы не даём уронить в ноль,
        // иначе фон гаснет (синь-красный и т.п.)
        if (bgc.r && r5 < 2) r5 = 5;
        if (bgc.g && g5 < 2) g5 = 5;
        if (bgc.b && b5 < 2) b5 = 5;
        leds[i] = CRGB(r5, g5, b5);

        if (stLife[i]) {
          if (stLife[i] > dec) stLife[i] -= dec;
          else stLife[i] = 0;
          stPh[i] += 6;                                // период пульса ~1,3 с
          byte pulse = 26 + (sin8(stPh[i]) * 229 >> 8); // 10–100% яркости
          byte b = (uint32_t)stLife[i] * 255 / 333;    // спад от жизни
          b = scale8(b, pulse);                        // пульсация поверх спада
           // компенсация showLeds: звезда не зависит от яркости гирлянды
          if (cfg.bright > 0) {
            uint16_t bc = (uint16_t)b * 255 / cfg.bright;
            b = (bc > 255) ? 255 : bc;
          }
          leds[i] += ColorFromPalette(paletteArr[curPal], stHue[i], b, LINEARBLEND);
        }
      }
      strip->showLeds(cfg.bright);
      return;
    }

// ===============================================================

// ================== семейство 2 — светлячки ===================

    if (thisEffect >= ACTIVE_PALETTES * 2) {
      byte flyPal = thisEffect % ACTIVE_PALETTES;   // палитра из номера эффекта
      byte flyFam = thisEffect / ACTIVE_PALETTES;   // номер семейства светлячков = 2
      if (flyFamInitFor != flyFam) {
        initFlies();
        flyFamInitFor = flyFam;
      }
      renderFlies(flyPal);
      strip->showLeds(cfg.bright);
      return;
    }

//================================================================

// ========================= Градиент / Перлин ===================

    byte curPal = thisEffect % ACTIVE_PALETTES;      // палитра из номера эффекта

    // --- случайное направление градиента: 45 / 90 / 135 / 180 градусов ---
    static byte dirInitFor = 255;
    static uint16_t dirT = 0;              //переменная направления
    static int16_t gnx = 256, gny = 256;   // вектор направления, ×256
    static uint16_t countPG = 0;           // отдельная фаза градиента

    if (thisEffect < ACTIVE_PALETTES) {
      // вход в эффект или подошло время — бросаем новый случайный угол
      if (dirInitFor != thisEffect || --dirT == 0) {
        dirInitFor = thisEffect;
        dirT = 1000 + random(333);              // авто смена 30–40 с
        switch (random(4)) {
          case 0: gnx = 256;  gny = 256;   break;  // 45°
          case 1: gnx = 0;    gny = 362;   break;  // 90°
          case 2: gnx = 256;  gny = -256;  break;  // 135°
          case 3: gnx = 362;  gny = 0;     break;  // 180°
        }
        fadeCount = 25;                    // кроссфейд на смене угла
      }
    }

    for (int i = 0; i < cfg.ledAm; i++) {
      byte idx;

      if (thisEffect < ACTIVE_PALETTES) {
        int32_t p = (int32_t)xy[i][0] * gnx + (int32_t)xy[i][1] * gny;  // проекция на направление
        p >>= 8;
        idx = countPG + p * scale / 100;
      } else {
        idx = inoise8(xy[i][0] * scale / 10, xy[i][1] * scale / 10, countP);
      }
      CRGB color = ColorFromPalette(paletteArr[curPal], idx, 255, LINEARBLEND);

      if (fadeCount) leds[i] = blend(leds[i], color, 40);
      else leds[i] = color;
    }
    if (fadeCount) fadeCount--;

    // прирост фаз: у градиента и перлина — свои законы скорости
     if (thisEffect < ACTIVE_PALETTES) countPG += speed / 40 + 1;   // 1–7 тиков: ползунок до середины плавнее
    else countP += (speed - 128) / 10;                             // перлин — как было

    strip->showLeds(cfg.bright);
  }
}
