//switcher.ino
void switchEff() {
  const byte TOTAL = ACTIVE_PALETTES * 5;   // 5 семейств × 18 палитр = 90

  for (byte i = 0; i < TOTAL; i++) {
    if (cfg.rndCh) {
      byte e;
      do { e = random(TOTAL); } while (TOTAL > 1 && e == curEff);
      curEff = e;
    } else if (++curEff >= TOTAL) curEff = 0;
    if (!cfg.favOnly || effs[curEff].fav) break;  // фильтр по избранным только при favOnly
  }
  // если избранных нет (или единственный избранный = текущий) — остаёмся
  // на последнем кандидате

  DEBUG("switch to: ");
  DEBUGLN(curEff);

  palSwitchTmr.restart();
}
