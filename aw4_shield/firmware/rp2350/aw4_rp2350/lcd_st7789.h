// Minimální ovladač displeje ST7789 (Waveshare RP2350-Touch-LCD-2, 240 x 320 na výšku).
// Ověřeno na desce: inverze zapnutá, pořadí barev RGB (naležato MADCTL 0x60).
// Na výšku: MADCTL 0x00, otočeno o 180° 0xC0.
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "aw4_fonts.h"

namespace lcd {

const uint8_t PIN_BL = 15, PIN_DC = 16, PIN_CS = 17, PIN_SCK = 18, PIN_MOSI = 19, PIN_RST = 20;
const uint16_t W = 240, H = 320;
inline uint8_t &madctl() { static uint8_t m = 0x00; return m; }
inline void setFlip(bool flip) { madctl() = flip ? 0xC0 : 0x00; }

// barvy RGB565
const uint16_t BLACK = 0x0000, WHITE = 0xFFFF, RED = 0xF800, GREEN = 0x07E0, DKGREEN = 0x0320, BLUE = 0x001F,
               YELLOW = 0xFFE0, ORANGE = 0xFD20, GREY = 0x8410, DKGREY = 0x2945, CYAN = 0x07FF;

inline SPISettings &spi() { static SPISettings s(40000000, MSBFIRST, SPI_MODE0); return s; }
inline uint8_t *lineBuf() { static uint8_t b[2 * 320]; return b; }

// Podsvícení 0–100 % (PWM na GPIO15).
inline void backlight(uint8_t pct) { analogWriteRange(100); analogWrite(PIN_BL, pct > 100 ? 100 : pct); }

inline void cmd(uint8_t c) { digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); SPI.transfer(c); digitalWrite(PIN_CS, HIGH); }
inline void data(const uint8_t *d, size_t n) { digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); SPI.transfer(d, nullptr, n); digitalWrite(PIN_CS, HIGH); }
inline void cmd1(uint8_t c, uint8_t v) { cmd(c); data(&v, 1); }

// Znovu zapíše nastavení displeje (natočení, inverze, formát, zapnutí) – bez mazání obrazu.
// Volá se periodicky: kdyby rušení rozhodilo registry, obraz se sám opraví.
inline void refreshRegs() {
  SPI.beginTransaction(spi());
  cmd(0x11);            // SLPOUT
  cmd1(0x3A, 0x55);     // 16 bitů na pixel
  cmd1(0x36, madctl()); // natočení
  cmd(0x21);            // INVON
  cmd(0x13);            // NORON
  cmd(0x29);            // DISPON
  SPI.endTransaction();
}

inline void begin() {
  pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, LOW);   // podsvícení až po vykreslení
  SPI.setSCK(PIN_SCK); SPI.setTX(PIN_MOSI);
  SPI.begin();
  pinMode(PIN_DC, OUTPUT);              // GPIO16 je i výchozí MISO SPI0 – vrátit na obyčejný výstup
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
  SPI.beginTransaction(spi()); cmd(0x01); SPI.endTransaction(); delay(150);   // SWRESET
  SPI.beginTransaction(spi()); cmd(0x11); SPI.endTransaction(); delay(120);   // SLPOUT
  refreshRegs();
}

inline void window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t x1 = x + w - 1, y1 = y + h - 1;
  uint8_t a[4] = {uint8_t(x >> 8), uint8_t(x), uint8_t(x1 >> 8), uint8_t(x1)};
  uint8_t b[4] = {uint8_t(y >> 8), uint8_t(y), uint8_t(y1 >> 8), uint8_t(y1)};
  cmd(0x2A); data(a, 4);
  cmd(0x2B); data(b, 4);
  cmd(0x2C);
}

inline void fillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;
  uint8_t *lb = lineBuf();
  for (int i = 0; i < w; i++) { lb[2 * i] = c >> 8; lb[2 * i + 1] = c; }
  SPI.beginTransaction(spi());
  window(x, y, w, h);
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW);
  for (int r = 0; r < h; r++) SPI.transfer(lb, nullptr, 2 * w);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
}

inline void frame(int x, int y, int w, int h, int t, uint16_t c) {
  fillRect(x, y, w, t, c); fillRect(x, y + h - t, w, t, c);
  fillRect(x, y, t, h, c); fillRect(x + w - t, y, t, h, c);
}

inline int textWidth(const AwFont &f, const char *s) {
  int w = 0;
  for (; *s; s++) if (*s >= f.first && *s <= f.last) w += f.glyphs[*s - f.first].width;
  return w;
}

// Vykreslí text i s pozadím (přepis bez blikání). Vrací šířku.
inline int text(int x, int y, const char *s, const AwFont &f, uint16_t fg, uint16_t bg) {
  int x0 = x;
  uint8_t *lb = lineBuf();
  for (; *s; s++) {
    if (*s < f.first || *s > f.last) continue;
    const AwGlyph &g = f.glyphs[*s - f.first];
    if (g.width == 0) continue;
    if (x + g.width > W || y + f.height > H) break;
    int bpr = (g.width + 7) / 8;
    SPI.beginTransaction(spi());
    window(x, y, g.width, f.height);
    digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW);
    const uint8_t *row = f.bitmap + g.offset;
    for (int r = 0; r < f.height; r++, row += bpr) {
      for (int i = 0; i < g.width; i++) {
        uint16_t c = (row[i >> 3] & (0x80 >> (i & 7))) ? fg : bg;
        lb[2 * i] = c >> 8; lb[2 * i + 1] = c;
      }
      SPI.transfer(lb, nullptr, 2 * g.width);
    }
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();
    x += g.width;
  }
  return x - x0;
}

// Text vystředěný v poli šířky w; zbytek pole se smaže barvou pozadí.
inline void textBox(int x, int y, int w, const char *s, const AwFont &f, uint16_t fg, uint16_t bg) {
  int tw = textWidth(f, s); if (tw > w) tw = w;
  int tx = x + (w - tw) / 2;
  fillRect(x, y, tx - x, f.height, bg);
  int drawn = text(tx, y, s, f, fg, bg);
  fillRect(tx + drawn, y, x + w - (tx + drawn), f.height, bg);
}

// ---- sedmisegmentová číslice ----
// Segment s „zkosenými" konci (šestiúhelník), kreslený po řádcích.
inline void segH(int x, int y, int len, int th, uint16_t c) {   // vodorovný
  int h2 = th / 2;
  for (int r = 0; r < th; r++) { int in = r < h2 ? h2 - r : r - h2; fillRect(x + in, y + r, len - 2 * in, 1, c); }
}
inline void segV(int x, int y, int len, int th, uint16_t c) {   // svislý
  int h2 = th / 2;
  for (int r = 0; r < len; r++) {
    int in = r < h2 ? h2 - r : (r > len - 1 - h2 ? r - (len - 1 - h2) : 0);
    fillRect(x + in, y + r, th - 2 * in, 1, c);
  }
}
// Segmenty a..g (bit 0 = a). Číslice 0–9, 10 = pomlčka, 11 = prázdno.
inline uint8_t segMask(int d) {
  static const uint8_t M[12] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x40, 0x00};
  return M[(d < 0 || d > 11) ? 11 : d];
}
// Vykreslí číslici do obdélníku (x, y, w, h); nesvítící segmenty barvou off. Překreslí jen změněné segmenty.
inline void sevenSeg(int x, int y, int w, int h, int th, uint8_t mask, uint8_t prevMask, uint16_t on, uint16_t off, bool force) {
  int g = th / 5 + 2;                       // mezera mezi segmenty
  int hl = w - th - 2 * g;                  // délka vodorovného
  int vl = (h - 3 * th) / 2 + th - 2 * g;   // délka svislého
  int xl = x, xr = x + w - th, xh = x + th / 2 + g;
  int ya = y, yg = y + (h - th) / 2, yd = y + h - th;
  int yv1 = y + th / 2 + g, yv2 = yg + th / 2 + g;
  for (int i = 0; i < 7; i++) {
    bool now = mask & (1 << i), was = prevMask & (1 << i);
    if (!force && now == was) continue;
    uint16_t c = now ? on : off;
    switch (i) {
      case 0: segH(xh, ya, hl, th, c); break;    // a
      case 1: segV(xr, yv1, vl, th, c); break;   // b
      case 2: segV(xr, yv2, vl, th, c); break;   // c
      case 3: segH(xh, yd, hl, th, c); break;    // d
      case 4: segV(xl, yv2, vl, th, c); break;   // e
      case 5: segV(xl, yv1, vl, th, c); break;   // f
      case 6: segH(xh, yg, hl, th, c); break;    // g
    }
  }
}

}  // namespace lcd
