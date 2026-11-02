//GyverTwink_v1.2_Plus.ino
/*
  Скетч к проекту "GyverTwink"
  - Страница проекта (схемы, описания): https://alexgyver.ru/gyvertwink/
  - Исходники на GitHub: https://github.com/AlexGyver/GyverTwink
  AlexGyver, AlexGyver Technologies, 2021
*/

/*
  Доработанно Annihilant 29.10.2026г.
  - Эффекты и палитры разделены: 5 типов × 18 палитр
    (тип = curEff / 18, палитра = curEff % 18)
  - Две автосмены: глобальная (все 90 комбинаций) и палитровая (в рамках типа)
  - Вкладки: web.ino, effects.ino, switcher.ino, Flies.ino,
    faders.ino, button.ino, parsing.ino, startup.ino, palettes.h
  - Исправлено: ползунки сбрасывали эффект на предыдущий (stale forceEff) — и в
    вебморде, и в UDP-парсере
  - Исправлено: случайный выбор мог повторить текущий эффект
  - Исправлено: автосмена и приложение видели только 54 из 90 комбинаций (* 3 → * 5)
  - «Следующий эффект» переключает тип, сохраняя палитру
  - Палитровая автосмена не щёлкает во время форс-показа
  - Убран конфликт порта 80 (WiFiServer → ESP8266WebServer в web.ino)
  - Защита буфера UDP и калибровки от выхода за границы массивов
*/

/*
  Мигает синим - открыт портал
  Мигает жёлтым - подключаемся к точке
  Мигнул зелёным - подключился к точке
  Мигнул красным - ошибка подключения к точке
  Мигнул розовым - создал точку
*/

// ================ НАСТРОЙКИ ================
#define BTN_PIN D3      // пин кнопки
#define BTN_TOUCH 0     // 1 - сенсорная кнопка, 0 - нет

#define LED_PIN D1      // пин ленты
#define LED_TYPE WS2812 // чип ленты
#define LED_ORDER RGB   // порядок цветов ленты
#define LED_MAX 500     // макс. светодиодов

// имя точки в режиме AP
#define GT_AP_SSID "GyverTwink"
#define GT_AP_PASS "12345678"
//#define DEBUG_SERIAL_GT   // раскомментируй, чтобы включить отладку

// ================== LIBS ==================
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SimplePortal.h>
#include <FastLED.h>
#include <EEManager.h>
#include <EncButton.h>
#include "palettes.h"
#include "Timer.h"

// ================== OBJECTS ==================
WiFiUDP udp;
EEManager EEwifi(portalCfg);
CRGB leds[LED_MAX];
CLEDController *strip;
EncButton<EB_TICK, BTN_PIN> btn;
IPAddress myIP;

// ========== прототипы функций из вкладок =====
// web.ino
void setupWeb();
void tickWeb();
byte curEffNow();          // её зовёт parsing.ino
// effects.ino
void effects();
// switcher.ino
void switchEff();
// Flies.ino
void initFlies();
void renderFlies(byte palIdx);
// faders.ino
void stepFader();
// parsing.ino
void parsing();
// button.ino
void button();
// startup.ino
// fireworks.ino
void salute(byte thisEffect, byte scale, byte speed);
void startStrip();
void setupAP();
void setupSTA();
void portalRoutine();
bool checkButton();

//====================================================

// ================== EEPROM BLOCKS ==================
struct Cfg {
  uint16_t ledAm = 50;
  bool power = 1;
  byte bright = 100;
  bool autoCh = 0;
  bool rndCh = 0;
  byte prdCh = 1;
  bool turnOff = 0;
  byte offTmr = 60;
  bool palAuto = 0;          // автосмена палитр в рамках эффекта
  bool favOnly = 1;          // автосмена только по избранным
};
Cfg cfg;
EEManager EEcfg(cfg);

byte xy[LED_MAX][2];
EEManager EExy(xy);

struct MM {
  byte minY = 0;
  byte maxY = 255;
  byte minX = 0;
  byte maxX = 255;
  byte w = 255;
  byte h = 255;
};
MM mm;
EEManager EEmm(mm);

#define ACTIVE_PALETTES 18
struct Effects {
  bool fav = true;
  byte scale = 200;
  byte speed = 200;
};
Effects effs[ACTIVE_PALETTES * 5];  // 5 типов × 18 палитр = 90
EEManager EEeff(effs);

// ================== MISC DATA ==================
Timer forceTmr(30000, false);
Timer switchTmr(0, false);
Timer palSwitchTmr(0, false);   // автосмена палитр в рамках текущего эффекта
Timer offTmr(60000, false);
bool calibF = false;
byte curEff = 0;
byte forceEff = 0;

// ================= FADER STATE =================
struct FaderState {
  CRGB color = CRGB::Black;
  int val = 0;
  int step = 3;
  bool dir = true;
  bool active = false;
  uint32_t lastTime = 0;
  byte mode = 0;
};
FaderState faderState;

void stopFader() {
  faderState.active = false;
  fill_solid(leds, cfg.ledAm, CRGB::Black);
  strip->showLeds(0);
}

#ifdef DEBUG_SERIAL_GT
#define DEBUGLN(x) Serial.println(x)
#define DEBUG(x) Serial.print(x)
#else
#define DEBUGLN(x)
#define DEBUG(x)
#endif

// ============ АВТОСМЕНА ПАЛИТР ============

void tickPalSwitch() {  // автосмена палитры внутри текущего типа эффекта
  byte fam = curEff / ACTIVE_PALETTES;   // тип (градиент, перлин, ...)
  byte p   = curEff % ACTIVE_PALETTES;   // текущая палитра
  byte np;
  bool favHasAny = false;                // есть ли избранные палитры в этом типе
  for (byte i = 0; i < ACTIVE_PALETTES; i++)
    if (effs[fam * ACTIVE_PALETTES + i].fav) { favHasAny = true; break; }

  if (cfg.favOnly && favHasAny) {        // крутимся только по избранным палитрам типа
    if (cfg.rndCh) {
      byte favCount = 0;
      for (byte i = 0; i < ACTIVE_PALETTES; i++)
        if (effs[fam * ACTIVE_PALETTES + i].fav) favCount++;
      if (favCount == 1) {               // единственная избранная — просто остаёмся
        for (byte i = 0; i < ACTIVE_PALETTES; i++)
          if (effs[fam * ACTIVE_PALETTES + i].fav) np = i;
      } else {
        do { np = random(ACTIVE_PALETTES); }
        while (np == p || !effs[fam * ACTIVE_PALETTES + np].fav);
      }
    } else {
      np = p;
      for (byte i = 1; i <= ACTIVE_PALETTES; i++) {   // шаг вперёд до ближайшей избранной
        np = (p + i) % ACTIVE_PALETTES;
        if (effs[fam * ACTIVE_PALETTES + np].fav) break;
      }
    }
  } else {                               // все палитры — прежняя логика
    if (cfg.rndCh) {
      do { np = random(ACTIVE_PALETTES); } while (ACTIVE_PALETTES > 1 && np == p);
    } else np = (p + 1) % ACTIVE_PALETTES;
  }
  curEff = fam * ACTIVE_PALETTES + np;   // тип прежний, палитра новая
  stopFader();
}

// ================== SETUP ==================
void setup() {
#ifdef DEBUG_SERIAL_GT
  Serial.begin(115200);
  DEBUGLN();
#endif
  delay(200);
  if (BTN_TOUCH) btn.setButtonLevel(HIGH);
  startStrip();     // startup.ino
  EEPROM.begin(2048); // с запасом!

  // ---- одноразовая очистка сектора от наследия старых раскладок ----
  // маркер 0x5A на последнем байте: если его нет — сектор ещё не чистился
  // (для полного сброса настроек замени 0x5A на 0x5B и перешей)
  if (EEPROM.read(2047) != 0x5B) {
    for (uint16_t i = 0; i < 2048; i++) EEPROM.write(i, 0xFF);
    EEPROM.write(2047, 0x5B);
    EEPROM.commit();
    DEBUGLN("EEPROM wiped");
  }

  // ---- фиксированные адреса и уникальные ключи ----
  // wifi: 0..159   (запас под рост portalCfg)
  // cfg:  160..175
  // mm:   176..191
  // xy:   192..1193 (LED_MAX × 2 = 1000 байт + ключ)
  // eff:  1216..1487 (90 эффектов × 3 байта + ключ)

#ifdef DEBUG_SERIAL_GT
  bool eW = EEwifi.begin(0, 'w');
  DEBUG("wifi: "); DEBUGLN(eW ? "NEW" : "ok");
  if (eW || checkButton()) portalRoutine();
#else
  if (EEwifi.begin(0, 'w') || checkButton()) portalRoutine();
#endif

  // создаём точку или подключаемся к AP
  if (portalCfg.mode == WIFI_AP || (portalCfg.mode == WIFI_STA && portalCfg.SSID[0] == '\0')) setupAP();
  else setupSTA();
  DEBUGLN(myIP);

#ifdef DEBUG_SERIAL_GT
  DEBUG("cfg: "); DEBUGLN(EEcfg.begin(160, 'c') ? "NEW" : "ok");
  DEBUG("mm:  "); DEBUGLN(EEmm.begin(176, 'm') ? "NEW" : "ok");
  DEBUG("xy:  "); DEBUGLN(EExy.begin(192, 'x') ? "NEW" : "ok");
  DEBUG("eff: "); DEBUGLN(EEeff.begin(1216, 'f') ? "NEW" : "ok");
  DEBUG("reset: "); DEBUGLN(ESP.getResetReason());
#else
  EEcfg.begin(160, 'c');
  EEmm.begin(176, 'm');
  EExy.begin(192, 'x');
  EEeff.begin(1216, 'f');
#endif

  switchTmr.setPrd(cfg.prdCh * 60000ul);
  if (cfg.autoCh) switchTmr.restart();
  palSwitchTmr.setPrd(cfg.prdCh * 60000ul);
  if (cfg.palAuto && !cfg.autoCh) palSwitchTmr.restart();
  switchEff();      // switcher.ino
  cfg.turnOff = false;
  strip->setLeds(leds, cfg.ledAm);
  udp.begin(8888);

  setupWeb();       // web.ino
}

// ================== LOOP ==================
void loop() {
  button();   // button.ino — опрос кнопки

  // менеджеры епром
  EEcfg.tick();
  EEeff.tick();

  parsing();  // parsing.ino — парсим udp

  tickWeb();  // web.ino

  // таймер принудительного показа эффектов
  if (forceTmr.ready()) {
    forceTmr.stop();
    curEff = forceEff;                      // форс-эффект становится текущим
    if (cfg.autoCh) switchTmr.restart();    // автосмена продолжает с чистого периода
    if (cfg.palAuto && !cfg.autoCh) palSwitchTmr.restart();  // синхронизируем палитровую
  }

  // форс выключен, автосмена включена и настало время менять эффект
  if (cfg.autoCh && !forceTmr.state() && switchTmr.ready()) switchEff();

  // форс выключен, палитровая автосмена включена и время пришло
  if (cfg.palAuto && !cfg.autoCh && !forceTmr.state() && palSwitchTmr.ready()) tickPalSwitch();

  // таймер выключения
  if (offTmr.ready()) {
    offTmr.stop();
    cfg.turnOff = false;
    cfg.power = false;
    strip->showLeds(0);
    EEcfg.update();
    DEBUGLN("Off tmr");
  }

  // показываем эффект, если включены
  if (!calibF) {
    if (faderState.active) stepFader();     // faders.ino — спецэффект в приоритете
    else if (cfg.power) effects();          // effects.ino — обычные эффекты
  }
}
