// Dotykové menu (řadič dotyku CST816 na I2C 0x15, GPIO12/13). Ťuknutí na hlavní obrazovce menu otevře.
// V menu: ťuknutím vybrat řádek, tlačítky - / + změnit hodnotu, ZPET uloží a zavře. Řazení běží dál.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "lcd_st7789.h"
#include "aw4_settings.h"

const uint8_t TOUCH_ADDR = 0x15, PIN_TP_SDA = 12, PIN_TP_SCL = 13;
const uint32_t MENU_TIMEOUT_MS = 20000;

bool menuOpen = false;
int menuSel = 0;
uint32_t menuLastMs = 0;
bool menuDirty = false;

inline void touchBegin() { Wire.setSDA(PIN_TP_SDA); Wire.setSCL(PIN_TP_SCL); Wire.begin(); Wire.setClock(400000); }

// Přečte dotyk v souřadnicích obrazovky (na výšku 240 x 320).
inline bool touchRead(int &x, int &y) {
  Wire.beginTransmission(TOUCH_ADDR); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDR, (size_t)5) != 5u) return false;
  uint8_t fingers = Wire.read(), xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (!fingers) return false;
  x = ((xh & 0x0F) << 8) | xl; y = ((yh & 0x0F) << 8) | yl;
  if (cfg.flip != cfg.touchInv) { x = lcd::W - 1 - x; y = lcd::H - 1 - y; }
  return x >= 0 && x < lcd::W && y >= 0 && y < lcd::H;
}

// ---- položky menu ----
const int MENU_N = 7, ROW_Y0 = 30, ROW_H = 32, BTN_Y = 262;
const char *const MENU_LABEL[MENU_N] = {"Lockup od st.", "Lockup od", "Lockup plyn", "Max ot. dolu", "Jas", "Otocit 180", "CAN rusEFI"};

inline void menuValue(int i, char *b, size_t n) {
  switch (i) {
    case 0: snprintf(b, n, "%u.", cfg.lockMinGear); break;
    case 1: snprintf(b, n, "%u km/h", cfg.lockMinKmh); break;
    case 2: snprintf(b, n, "<%u%%", cfg.lockMaxTps); break;
    case 3: snprintf(b, n, "%u", cfg.downMaxRpm); break;
    case 4: snprintf(b, n, "%u%%", cfg.bright); break;
    case 5: snprintf(b, n, "%s", cfg.flip ? "ano" : "ne"); break;
    case 6: snprintf(b, n, "%s", cfg.canOn ? "zap" : "vyp"); break;
  }
}
template <typename T> inline void step(T &v, int d, int lo, int hi) { int x = int(v) + d; v = T(x < lo ? lo : x > hi ? hi : x); }
// Změní hodnotu; vrací true, když je potřeba překreslit celou obrazovku (otočení).
inline bool menuChange(int i, int dir) {
  switch (i) {
    case 0: step(cfg.lockMinGear, dir, 3, 4); break;
    case 1: step(cfg.lockMinKmh, 5 * dir, 0, 150); break;
    case 2: step(cfg.lockMaxTps, 5 * dir, 20, 100); break;
    case 3: step(cfg.downMaxRpm, 100 * dir, 3000, 7000); break;
    case 4: step(cfg.bright, 10 * dir, 10, 100); lcd::backlight(cfg.bright); break;
    case 5: cfg.flip = !cfg.flip; lcd::setFlip(cfg.flip); lcd::refreshRegs(); return true;
    case 6: cfg.canOn = !cfg.canOn; break;
  }
  menuDirty = true;
  return false;
}

using namespace lcd;
inline void menuDrawRow(int i) {
  int y = ROW_Y0 + i * ROW_H;
  uint16_t bg = i == menuSel ? 0x0320 : BLACK;
  fillRect(0, y, W, ROW_H - 2, bg);
  text(4, y + 3, MENU_LABEL[i], fontSmall, WHITE, bg);
  char v[16]; menuValue(i, v, sizeof v);
  text(W - 4 - textWidth(fontSmall, v), y + 3, v, fontSmall, i == menuSel ? GREEN : YELLOW, bg);
}
inline void menuButton(int x, int w, const char *t, uint16_t c) {
  frame(x, BTN_Y, w, H - BTN_Y, 2, c);
  fillRect(x + 2, BTN_Y + 2, w - 4, H - BTN_Y - 4, BLACK);
  text(x + (w - textWidth(fontSmall, t)) / 2, BTN_Y + (H - BTN_Y - fontSmall.height) / 2, t, fontSmall, c, BLACK);
}
inline void menuDraw() {
  fillRect(0, 0, W, H, BLACK);
  text((W - textWidth(fontSmall, "NASTAVENI")) / 2, 2, "NASTAVENI", fontSmall, CYAN, BLACK);
  for (int i = 0; i < MENU_N; i++) menuDrawRow(i);
  menuButton(0, 78, "-", GREEN); menuButton(81, 78, "ZPET", WHITE); menuButton(162, 78, "+", GREEN);
}
inline void menuOpenNow(uint32_t now) { menuOpen = true; menuSel = 0; menuDirty = false; menuLastMs = now; menuDraw(); }
// Zavře menu (uloží změny). Hlavní obrazovku překreslí volající.
inline void menuClose() { if (menuDirty) cfgSave(); menuOpen = false; menuDirty = false; }

// Zpracuje ťuknutí v menu. Vrací true, když se má menu zavřít.
inline bool menuTap(int x, int y, uint32_t now) {
  menuLastMs = now;
  if (y >= BTN_Y) {
    if (x < 80 || x >= 161) {
      if (menuChange(menuSel, x < 80 ? -1 : 1)) menuDraw(); else menuDrawRow(menuSel);
      return false;
    }
    return true;   // ZPET
  }
  int i = (y - ROW_Y0) / ROW_H;
  if (y >= ROW_Y0 && i >= 0 && i < MENU_N && i != menuSel) { int o = menuSel; menuSel = i; menuDrawRow(o); menuDrawRow(i); }
  return false;
}
