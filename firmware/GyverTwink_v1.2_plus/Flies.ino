//Flies.ino
#define FLY_NUM 6

struct Fly {
  float x, y;      // позиция в калиброванных координатах
  float dx, dy;    // скорость дрейфа
  byte phase;      // фаза мерцания
  byte speed;      // скорость мерцания
  byte hue;        // позиция в палитре (цвет светлячка)
};

Fly flies[FLY_NUM];

void initFlies() {
  for (byte i = 0; i < FLY_NUM; i++) {
    flies[i].x = mm.minX + random(mm.w);
    flies[i].y = mm.minY + random(mm.h);
    flies[i].dx = (random(9) - 4) / 16.0;   // до ~0.25 мм за кадр
    flies[i].dy = (random(9) - 4) / 16.0;
    flies[i].phase = random(255);           // рассинхрон мерцания
    flies[i].speed = 1 + random(4);
    flies[i].hue = random8();
  }
}

void renderFlies(byte palIdx) {
  // мягкое затухание всего кадра — шлейф и тёмный фон
  for (int i = 0; i < cfg.ledAm; i++) leds[i].fadeToBlackBy(12);

  for (byte f = 0; f < FLY_NUM; f++) {
    Fly &fl = flies[f];

    // дрейф с отражением от границ калибровки
    fl.x += fl.dx;
    fl.y += fl.dy;
    if (fl.x < mm.minX || fl.x > mm.maxX) { fl.dx = -fl.dx; fl.x = constrain(fl.x, mm.minX, mm.maxX); }
    if (fl.y < mm.minY || fl.y > mm.maxY) { fl.dy = -fl.dy; fl.y = constrain(fl.y, mm.minY, mm.maxY); }

    // плавное появление/исчезание
    fl.phase += fl.speed;
    byte bri = sin8(fl.phase);
    if (bri < 10) continue;   // «спит» — диод даже не ищем

    // ближайший диод к точке светлячка
    int best = 0;
    long bestD = 0x7FFFFFFF;
    for (int i = 0; i < cfg.ledAm; i++) {
      long d = sq((long)xy[i][0] - (long)fl.x) + sq((long)xy[i][1] - (long)fl.y);
      if (d < bestD) { bestD = d; best = i; }
    }

    CRGB c = ColorFromPalette(paletteArr[palIdx], fl.hue, bri, LINEARBLEND);
    leds[best] += c;          // аддитивно: встреча светлячков = яркое пятно
  }
}
