// fireworks.ino — семейство 4: салют (точка → взрыв кольцом → осыпание)

#define FW_SLOTS 7      // максимум одновременных салютов
#define FW_SPARKS 32    // потолок частиц осыпания
#define FW_GS 16        // ячеек сетки на сторону

void salute(byte thisEffect, byte scale, byte speed) {
  byte pal = thisEffect % ACTIVE_PALETTES;   // палитра из номера эффекта

  struct FwP { int16_t x, y, vx, vy; uint8_t life; };
  struct Fw {
    FwP s[FW_SPARKS];
    byte phase;     // 0 точка 10% · 1 разгон · 2 затухание · 3 рост кольца · 4 осыпание · 255 свободно
    byte phaseT;    // тики в текущей фазе
    byte growT;     // длительность роста кольца, тиков (зависит от периода)
    byte n;         // сколько частиц реально осыпается
    int16_t r8;     // текущий радиус кольца, ×8
    int16_t rMax8;  // максимальный радиус, ×8
    uint8_t col;    // цвет всего залпа
    byte center;
  };
  static Fw fw[FW_SLOTS];
  static byte fwInitFor = 255;   // семейство, для которого инициализированы пул и сетка
  static byte spawnC = 60;
  static uint16_t band8 = 24;                     // толщина кольца, ×8
  static int16_t fwMinX, fwMaxX, fwMinY, fwMaxY;  // границы поля в ×8

  // --- пространственная сетка для поиска диода частицами ---
  static uint16_t fwHead[FW_GS * FW_GS];
  static uint16_t *fwNext = nullptr;
  static int fwGridLed = 0;

  auto buildGrid = [&]() {
    if (fwNext) delete[] fwNext;
    fwNext = new uint16_t[cfg.ledAm];
    for (int c = 0; c < FW_GS * FW_GS; c++) fwHead[c] = 0xFFFF;
    for (int j = cfg.ledAm - 1; j >= 0; j--) {
      int c = (xy[j][1] >> 4) * FW_GS + (xy[j][0] >> 4);
      fwNext[j] = fwHead[c];
      fwHead[c] = j;
    }
    fwGridLed = cfg.ledAm;
  };

  // ближайший к точке (px, py) диод: кольца ячеек + ранняя остановка
  auto nearestLed = [&](int16_t px, int16_t py) -> int {
    int gx = constrain(px >> 7, 0, FW_GS - 1);
    int gy = constrain(py >> 7, 0, FW_GS - 1);
    int32_t best = 0x7FFFFFFF;
    int bestI = -1;
    for (int r = 0; r < FW_GS; r++) {
      if (r > 0 && (int32_t)(r - 1) * (r - 1) * 128 * 128 > best) break;
      int x0 = gx - r, x1 = gx + r, y0 = gy - r, y1 = gy + r;
      for (int cx = x0; cx <= x1; cx++) {
        for (int cy = y0; cy <= y1; cy++) {
          if (r > 0 && cx != x0 && cx != x1 && cy != y0 && cy != y1) continue;
          if (cx < 0 || cx >= FW_GS || cy < 0 || cy >= FW_GS) continue;
          for (uint16_t j = fwHead[cy * FW_GS + cx]; j != 0xFFFF; j = fwNext[j]) {
            int16_t dx = ((int16_t)xy[j][0] << 3) - px;
            int16_t dy = ((int16_t)xy[j][1] << 3) - py;
            int32_t d = (int32_t)dx * dx + (int32_t)dy * dy;
            if (d < best) { best = d; bestI = j; }
          }
        }
      }
    }
    if (bestI < 0) {          // страховка: сетка не нашла — честный полный перебор
      for (int j = 0; j < cfg.ledAm; j++) {
        int16_t dx = ((int16_t)xy[j][0] << 3) - px;
        int16_t dy = ((int16_t)xy[j][1] << 3) - py;
        int32_t d = (int32_t)dx * dx + (int32_t)dy * dy;
        if (d < best) { best = d; bestI = j; }
      }
    }
    return bestI;
  };

  byte fam = thisEffect / ACTIVE_PALETTES;   // семейство (внутри salute всегда 4)
  if (fwInitFor != fam || fwGridLed != cfg.ledAm) {  // смена СЕМЕЙСТВА или числа диодов — чистим и строим заново
    fwInitFor = fam;
    for (byte s = 0; s < FW_SLOTS; s++) fw[s].phase = 255;
    int16_t mnx = 255, mxx = 0, mny = 255, mxy = 0;
    for (int i = 0; i < cfg.ledAm; i++) {
      mnx = min(mnx, (int16_t)xy[i][0]); mxx = max(mxx, (int16_t)xy[i][0]);
      mny = min(mny, (int16_t)xy[i][1]); mxy = max(mxy, (int16_t)xy[i][1]);
    }
    fwMinX = mnx << 3; fwMaxX = mxx << 3;
    fwMinY = mny << 3; fwMaxY = mxy << 3;
    int32_t area = (int32_t)(mxx - mnx + 1) * (int32_t)(mxy - mny + 1);
    int spacing = (int)sqrt((float)area / cfg.ledAm) + 1;
    band8 = constrain(spacing * 3, 8, 24);    // тонкое кольцо ~0,75 шага — меньше осыпается  //(spacing * 6, 12, 48);   // полная толщина кольца ~1,5 шага между диодами
    buildGrid();
    spawnC = 60;
  }

  // холст тает быстро — след кольца гаснет за ~40 мс
  for (int i = 0; i < cfg.ledAm; i++) leds[i].nscale8(150);

  // запуск нового салюта: период от ползунка скорости + случайность,
  // центр — минимум за rMax от краёв, чтобы кольцо не резалось о границу
  if (--spawnC == 0) {
    uint16_t pMs = 4000 - (uint32_t)speed * 3800 / 255;  // 0,2–4 с
    pMs += random(400) - 200;                           // ±0,2 с случайно
    pMs = constrain(pMs, 100, 4000);
    spawnC = pMs / 30;
    for (byte s = 0; s < FW_SLOTS; s++) {
      if (fw[s].phase == 255) {
        Fw &f = fw[s];
        f.phase = 0;
        f.phaseT = 0;
        f.growT = constrain(pMs / 120, 4, 40);   // скорость рост кольца
        f.r8 = 0;
        f.rMax8 = ((scale * 10) / 255) << 3;    // ползунок масштаба — максимальный радиус
        if (f.rMax8 < 16) f.rMax8 = 16;
        f.col = random8();                      // у всего залпа один цвет
        // ищем диод не ближе rMax к краям поля; узкое поле — берём любую точку
        f.center = random(cfg.ledAm);
        for (byte tries = 0; tries < 20; tries++) {
          byte c = random(cfg.ledAm);
          int16_t lx = (int16_t)xy[c][0] << 3;
          int16_t ly = (int16_t)xy[c][1] << 3;
          if (lx >= fwMinX + f.rMax8 && lx <= fwMaxX - f.rMax8 &&
              ly >= fwMinY + f.rMax8 && ly <= fwMaxY - f.rMax8) {
            f.center = c;
            break;
          }
          f.center = c;   // за 20 попыток не нашли — оставляем последнюю точку
        }
        break;
      }
    }
  }

  for (byte s = 0; s < FW_SLOTS; s++) {
    Fw &f = fw[s];
    if (f.phase == 255) continue;

    if (f.phase == 0) {               // точка на 10% яркости, 0,5 с
      leds[f.center] += ColorFromPalette(paletteArr[pal], f.col, 26, LINEARBLEND);
      if (++f.phaseT >= 17) { f.phase = 1; f.phaseT = 0; }
    }
    else if (f.phase == 1) {          // разгорание до максимума за 0,5 с
      byte b = 26 + (uint16_t)f.phaseT * 229 / 17;
      leds[f.center] += ColorFromPalette(paletteArr[pal], f.col, b, LINEARBLEND);
      if (++f.phaseT >= 17) { f.phase = 2; f.phaseT = 0; }
    }
    else if (f.phase == 2) {          // затухание точки за 0,5 с
      byte b = 255 - (uint16_t)f.phaseT * 255 / 17;
      leds[f.center] += ColorFromPalette(paletteArr[pal], f.col, b, LINEARBLEND);
      if (++f.phaseT >= 17) { f.phase = 3; f.phaseT = 0; }
    }
    else if (f.phase == 3) {          // кольцо растёт до максимального радиуса
      f.r8 = (int16_t)((int32_t)f.rMax8 * f.phaseT / f.growT);
      if (++f.phaseT > f.growT) {
        // кольцо дошло до максимума — осыпаются РЕАЛЬНО подсвеченные диоды
        f.phase = 4;
        f.phaseT = 0;
        int16_t cx = (int16_t)xy[f.center][0] << 3;
        int16_t cy = (int16_t)xy[f.center][1] << 3;
        byte n = 0;
        for (int j = 0; j < cfg.ledAm; j++) {
          int16_t dx = ((int16_t)xy[j][0] << 3) - cx;
          int16_t dy = ((int16_t)xy[j][1] << 3) - cy;
          int32_t d2 = (int32_t)dx * dx + (int32_t)dy * dy;
          if (d2 > 65535) continue;
          int16_t d = (int16_t)sqrt16((uint16_t)d2);
          if (abs(d - f.rMax8) > band8) continue;   // не в кольце — пропускаем
          FwP p;
          p.x = (int16_t)xy[j][0] << 3;             // падает ровно со своего места
          p.y = (int16_t)xy[j][1] << 3;
          int32_t kick = random(4, 10);                              // радиальный пинок: залп распускается кнаружи
          p.vx = (int16_t)(dx * kick / f.rMax8) + random(-1, 2);
          p.vy = (int16_t)(dy * kick / f.rMax8);                     // верхняя часть уходит вверх — потом заберёт гравитация
          p.life = random(20, 55);                        // 0,6–1,65 с падения
          if (n < FW_SPARKS) f.s[n] = p;
          else f.s[random(FW_SPARKS)] = p;                // больше заданного — случайный отбор
          n++;
        }
        f.n = (n > FW_SPARKS) ? FW_SPARKS : n;
      }
    }
    else if (f.phase == 4) {                        // осыпание: искры падают медленно, искрятся, угасают
          bool alive = false;
          for (byte i = 0; i < f.n; i++) {
            if (!f.s[i].life) continue;
            alive = true;
            if (f.s[i].life > 1) f.s[i].life -= 1;  // угасание
            else f.s[i].life = 0;
            f.s[i].vx = (f.s[i].vx * 12) / 16;      // дрейф тянется дольше — красивее пинок.   Было "8) / 16;"
            f.s[i].vy = min(f.s[i].vy + 1, 2);      // разгон вниз "1, 2", "1, 5" быстрее
        f.s[i].x += f.s[i].vx;
        f.s[i].y += f.s[i].vy;
        if (!f.s[i].life) continue;
        if (f.s[i].x < fwMinX || f.s[i].x > fwMaxX || f.s[i].y < fwMinY || f.s[i].y > fwMaxY) {
          f.s[i].life = 0;
          continue;
        }
        int bestI = nearestLed(f.s[i].x, f.s[i].y);
        if (bestI < 0) continue;
        uint8_t br = (uint16_t)f.s[i].life * 255 / 55;   // полная яркость при рождении
        // базовая искра — цвета салюта с лёгким дрожанием оттенка
        CRGB c = ColorFromPalette(paletteArr[pal], f.col + random8(24) - 12, br, LINEARBLEND);
        if (random8() < 60) {                      // короткая белая вспышка поверх цвета
          byte fb = random8(90, 160);
          c.r = qadd8(c.r, fb);
          c.g = qadd8(c.g, fb);
          c.b = qadd8(c.b, fb);
        }
        else if (random8() < 40) c.nscale8(128);   // лёгкое подрагивание яркости
        leds[bestI] += c;
      }
      if (!alive) f.phase = 255;
    }
  }

  // кольца активных залпов: яркая дуга на текущем радиусе
  for (int j = 0; j < cfg.ledAm; j++) {
    int16_t lx = (int16_t)xy[j][0] << 3;
    int16_t ly = (int16_t)xy[j][1] << 3;
    for (byte s = 0; s < FW_SLOTS; s++) {
      Fw &f = fw[s];
      if (f.phase != 3) continue;
      int16_t dx = lx - (int16_t)xy[f.center][0] * 8;
      int16_t dy = ly - (int16_t)xy[f.center][1] * 8;
      int32_t d2 = (int32_t)dx * dx + (int32_t)dy * dy;
      if (d2 > 65535) continue;
      int16_t d = (int16_t)sqrt16((uint16_t)d2);
      int16_t e = abs(d - f.r8);
      if (e <= band8) {
        int32_t q = (int32_t)e * e * 255 / ((int32_t)band8 * band8);   // квадратичный спад к краям
        leds[j] += ColorFromPalette(paletteArr[pal], f.col, 255 - q, LINEARBLEND);
      }
    }
  }

  strip->showLeds(cfg.bright);
}
