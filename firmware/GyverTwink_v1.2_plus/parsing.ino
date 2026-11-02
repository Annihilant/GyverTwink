// parsing.ino
#define MAX_UDP_PACKET 30
byte ubuf[MAX_UDP_PACKET];

// === неблокирующий отложенный ответ на запрос IP ===
bool answerPending = false;
uint32_t answerTmr = 0;

void reply(byte* data, byte size) {
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write("GT");
  udp.write(data, size);
  udp.endPacket();
}

void parsing() {
  static uint32_t tmr;
  byte answ[10];
  if (udp.parsePacket()) {
    if (millis() - tmr < 50) {  // "антидребезг" приёма
      udp.read(ubuf, MAX_UDP_PACKET);
      return;
    }
    tmr = millis();
    int n = udp.read(ubuf, MAX_UDP_PACKET);
    if (n < 0) return;
    if (n > MAX_UDP_PACKET - 1) n = MAX_UDP_PACKET - 1;  // не пишем за границу буфера
    ubuf[n] = 0;
    if (ubuf[0] != 'G' || ubuf[1] != 'T') return;

    switch (ubuf[2]) {
      case 0:   // запрос IP — откладываем ответ вместо delay
        answerTmr = millis() + myIP[3] * 2;
        answerPending = true;
        break;

      case 1:   // запрос настроек (протокол приложения не меняем — ровно 10 байт)
        answ[0] = 1;
        answ[1] = cfg.ledAm / 100;
        answ[2] = cfg.ledAm % 100;
        answ[3] = cfg.power;
        answ[4] = cfg.bright;
        answ[5] = cfg.autoCh;
        answ[6] = cfg.rndCh;
        answ[7] = cfg.prdCh;
        answ[8] = cfg.turnOff;
        answ[9] = cfg.offTmr;
        reply(answ, 10);
        break;

      case 2:   // приём настроек
        forceTmr.stop();
        switch (ubuf[3]) {
          case 0: cfg.ledAm = ubuf[4] * 100 + ubuf[5];
            strip->setLeds(leds, cfg.ledAm);
            break;
          case 1: cfg.power = ubuf[4];
            // если выключили из приложения — глушим и спецэффект
            if (!cfg.power) stopFader();
            break;
          case 2: cfg.bright = ubuf[4];
            break;
          case 3: cfg.autoCh = ubuf[4];
            // связка с палитровой автосменой, как в вебморде
            if (cfg.autoCh) { switchTmr.restart(); palSwitchTmr.stop(); }
            else {
              switchTmr.stop();
              if (cfg.palAuto) palSwitchTmr.restart();
              else palSwitchTmr.stop();
            }
            break;
          case 4: cfg.rndCh = ubuf[4];
            break;
          case 5: cfg.prdCh = ubuf[4];
            // период задаём обоим таймерам
            switchTmr.setPrd(cfg.prdCh * 60000ul);
            palSwitchTmr.setPrd(cfg.prdCh * 60000ul);
            if (cfg.autoCh) switchTmr.restart();
            else if (cfg.palAuto) palSwitchTmr.restart();
            break;
          case 6:   // нехт эффект
            stopFader();               // мигание не перекрывает новый эффект
            switchEff();
            if (cfg.autoCh) switchTmr.restart();
            return;
            break;
          case 7:
            cfg.turnOff = ubuf[4];
            if (cfg.turnOff) offTmr.restart();
            else offTmr.stop();
            break;
          case 8:
            cfg.offTmr = ubuf[4];
            offTmr.setPrd(cfg.offTmr * 60000ul);
            if (cfg.turnOff) offTmr.restart();
            break;
        }
        if (!cfg.power) strip->showLeds(0);
        EEcfg.update();
        break;

      case 3:
        switch (ubuf[3]) {
          case 0:   // запуск калибровки
            DEBUGLN("Calibration start");
            calibF = true;
            stopFader();          // не даём фейдеру мешать калибровке
            strip->clearLedData();
            strip->showLeds(0);
            break;

          case 1:   // следующий лед
            {
              int curLed = ubuf[4] * 100 + ubuf[5];
              if (curLed > 0 && curLed <= LED_MAX) {   // защита от выхода за xy[]
                xy[curLed - 1][0] = ubuf[6];
                xy[curLed - 1][1] = ubuf[7];
              }
              strip->clearLedData();
              if (curLed >= 0 && curLed < LED_MAX) leds[curLed] = CRGB::White;
              strip->showLeds(200);
            }
            break;

          case 2:   // калибровка окончена
            DEBUGLN("Finished");
            calibF = false;
            strip->clearLedData();
            strip->showLeds(0);
            EExy.updateNow();

            mm.minY = 255;
            mm.maxY = 0;
            mm.minX = 255;
            mm.maxX = 0;
            for (int i = 0; i < cfg.ledAm; i++) {
              mm.minX = min(mm.minX, xy[i][0]);
              mm.maxX = max(mm.maxX, xy[i][0]);
              mm.minY = min(mm.minY, xy[i][1]);
              mm.maxY = max(mm.maxY, xy[i][1]);
            }
            mm.w = mm.maxX - mm.minX;
            mm.h = mm.maxY - mm.minY;
            if (mm.h == 0) mm.h = 255;   // защита от деления на ноль
            if (mm.w == 0) mm.w = 255;
            EEmm.updateNow();
            break;
        }
        break;

      case 4:   // управление эффектами
        switch (ubuf[3]) {
          case 0:   // выбор эффекта в дропе
            if (ubuf[4] < ACTIVE_PALETTES * 5) {      // только валидный номер
              forceEff = ubuf[4];
              curEff = forceEff;      // показываем сразу, а не через 30 с форса
              forceTmr.restart();
              answ[0] = 4;
              answ[1] = effs[forceEff].fav;
              answ[2] = effs[forceEff].scale;
              answ[3] = effs[forceEff].speed;
              reply(answ, 4);
            }
            break;
          case 1:   // флажок избранное
          case 2:   // масштаб
          case 3:   // скорость
            // форс ещё жив — работаем с форс-эффектом; истёк — актуализируем номер
            if (!forceTmr.state()) forceEff = curEff;
            forceTmr.restart();
            switch (ubuf[3]) {
              case 1: effs[curEffNow()].fav = ubuf[4];
                answ[0] = 4;
                answ[1] = effs[curEffNow()].fav;
                answ[2] = effs[curEffNow()].scale;
                answ[3] = effs[curEffNow()].speed;
                reply(answ, 4);
                break;
              case 2: effs[curEffNow()].scale = ubuf[4];
                break;
              case 3: effs[curEffNow()].speed = ubuf[4];
                break;
            }
            break;
        }
        EEeff.update();
        break;

      // ============ спецэффекты по UDP ============
      case 5:
        switch (ubuf[3]) {
          case 0: {  // запустить мигание, цвет из ubuf[4..6] (R, G, B)
            CRGB c = CRGB(ubuf[4], ubuf[5], ubuf[6]);
            startFadeBlink(c);
            answ[0] = 5;
            reply(answ, 1);
            break;
          }
          case 1:    // остановить спецэффект
            stopFader();
            answ[0] = 5;
            reply(answ, 1);
            break;
        }
        break;
    }
  }

  // отложенный ответ "созрел" — отправляем без блокировки
  if (answerPending && millis() - answerTmr < 0x80000000) {
    if ((int32_t)(millis() - answerTmr) >= 0) {
      answ[0] = 0;
      answ[1] = myIP[3];
      reply(answ, 2);
      answerPending = false;
    }
  }
}
