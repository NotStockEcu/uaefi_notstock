#include <Arduino.h>
#include <SPI.h>
#include <initializer_list>
#include <Wire.h>
#include "aw4_fonts.h"

const uint8_t PIN_BL = 15, PIN_DC = 16, PIN_CS = 17, PIN_SCK = 18, PIN_MOSI = 19, PIN_RST = 20;
const uint8_t PIN_SDA = 12, PIN_SCL = 13, PIN_TP_INT = 29;
const uint8_t TOUCH_ADDR = 0x15;          // CST816 (předpoklad – ověří sken I2C)

// barvy RGB565
const uint16_t BLACK = 0x0000, WHITE = 0xFFFF, RED = 0xF800, GREEN = 0x07E0, BLUE = 0x001F,
               YELLOW = 0xFFE0, ORANGE = 0xFD20, GREY = 0x8410, CYAN = 0x07FF;

// natočení: MADCTL hodnoty pro naležato (MV) a nastojato
const uint8_t MADCTL_OPTS[] = {0x60, 0xA0, 0x20, 0xE0, 0x00, 0xC0};
uint8_t madIdx = 0;
bool invert = true, bgr = false;
uint16_t W = 320, H = 240;

SPISettings spiSet(40000000, MSBFIRST, SPI_MODE0);

// ---------------- ST7789 ----------------
void lcdCmd(uint8_t c) { digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); SPI.transfer(c); digitalWrite(PIN_CS, HIGH); }
void lcdData(const uint8_t *d, size_t n) { digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); SPI.transfer(d, nullptr, n); digitalWrite(PIN_CS, HIGH); }
void lcdCmdData(uint8_t c, std::initializer_list<uint8_t> d) { lcdCmd(c); uint8_t b[8]; size_t n = 0; for (auto v : d) b[n++] = v; if (n) lcdData(b, n); }

void lcdApplyRotation() {
  uint8_t m = MADCTL_OPTS[madIdx] | (bgr ? 0x08 : 0x00);
  SPI.beginTransaction(spiSet);
  lcdCmdData(0x36, {m});
  lcdCmd(invert ? 0x21 : 0x20);
  SPI.endTransaction();
  bool landscape = MADCTL_OPTS[madIdx] & 0x20;
  W = landscape ? 320 : 240; H = landscape ? 240 : 320;
}

void lcdInit() {
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
  SPI.beginTransaction(spiSet);
  lcdCmd(0x01); delay(150);             // SWRESET
  lcdCmd(0x11); delay(120);             // SLPOUT
  lcdCmdData(0x3A, {0x55});             // 16 bitů na pixel
  lcdCmd(0x13);                         // NORON
  SPI.endTransaction();
  lcdApplyRotation();
  SPI.beginTransaction(spiSet); lcdCmd(0x29); SPI.endTransaction();   // DISPON
}

void lcdWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t x1 = x + w - 1, y1 = y + h - 1;
  lcdCmdData(0x2A, {uint8_t(x >> 8), uint8_t(x), uint8_t(x1 >> 8), uint8_t(x1)});
  lcdCmdData(0x2B, {uint8_t(y >> 8), uint8_t(y), uint8_t(y1 >> 8), uint8_t(y1)});
  lcdCmd(0x2C);
}

uint8_t lineBuf[2 * 320];

void fillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;
  for (int i = 0; i < w; i++) { lineBuf[2 * i] = c >> 8; lineBuf[2 * i + 1] = c; }
  SPI.beginTransaction(spiSet);
  lcdWindow(x, y, w, h);
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW);
  for (int r = 0; r < h; r++) SPI.transfer(lineBuf, nullptr, 2 * w);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
}

int textWidth(const AwFont &f, const char *s) {
  int w = 0;
  for (; *s; s++) if (*s >= f.first && *s <= f.last) w += f.glyphs[*s - f.first].width;
  return w;
}

// Vykreslí text; vrací šířku. Pozadí se kreslí (žádné blikání při přepisu).
int drawText(int x, int y, const char *s, const AwFont &f, uint16_t fg, uint16_t bg) {
  int x0 = x;
  for (; *s; s++) {
    if (*s < f.first || *s > f.last) continue;
    const AwGlyph &g = f.glyphs[*s - f.first];
    if (g.width == 0) continue;
    if (x + g.width > W || y + f.height > H) break;
    int bpr = (g.width + 7) / 8;
    SPI.beginTransaction(spiSet);
    lcdWindow(x, y, g.width, f.height);
    digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW);
    const uint8_t *row = f.bitmap + g.offset;
    for (int r = 0; r < f.height; r++, row += bpr) {
      for (int i = 0; i < g.width; i++) {
        uint16_t c = (row[i >> 3] & (0x80 >> (i & 7))) ? fg : bg;
        lineBuf[2 * i] = c >> 8; lineBuf[2 * i + 1] = c;
      }
      SPI.transfer(lineBuf, nullptr, 2 * g.width);
    }
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();
    x += g.width;
  }
  return x - x0;
}

void drawTextCentered(int cx, int y, const char *s, const AwFont &f, uint16_t fg, uint16_t bg) {
  drawText(cx - textWidth(f, s) / 2, y, s, f, fg, bg);
}

// ---------------- dotyk / I2C ----------------
bool touchFound = false;

void scanI2C() {
  Serial.println(F("I2C sken (GPIO12/13) – behem skenu se dotknete displeje, dotyk se jinak muze uspavat:"));
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  nalezeno 0x")); if (a < 16) Serial.print('0'); Serial.print(a, HEX);
      if (a == 0x15) { Serial.print(F("  = radic dotyku (CST816)")); touchFound = true; }
      if (a == 0x6A || a == 0x6B) Serial.print(F("  = IMU QMI8658"));
      Serial.println(); n++;
    }
  }
  if (!n) Serial.println(F("  nic nenalezeno"));
}

bool readTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(TOUCH_ADDR); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDR, (size_t)5) != 5u) return false;
  uint8_t fingers = Wire.read(), xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (!fingers) return false;
  x = ((xh & 0x0F) << 8) | xl; y = ((yh & 0x0F) << 8) | yl;
  return true;
}

// ---------------- testovací obrazovka ----------------
char info[48];

bool landscape() { return W > H; }

void drawScreen() {
  fillRect(0, 0, W, H, BLACK);
  // rohy: barevný čtvereček + popis
  fillRect(0, 0, 14, 14, RED);              drawText(16, 0, "TL", fontSmall, RED, BLACK);
  fillRect(W - 14, 0, 14, 14, GREEN);       drawText(W - 16 - textWidth(fontSmall, "TR"), 0, "TR", fontSmall, GREEN, BLACK);
  fillRect(0, H - 14, 14, 14, BLUE);        drawText(16, H - fontSmall.height, "BL", fontSmall, BLUE, BLACK);
  fillRect(W - 14, H - 14, 14, 14, WHITE);  drawText(W - 16 - textWidth(fontSmall, "BR"), H - fontSmall.height, "BR", fontSmall, WHITE, BLACK);
  drawTextCentered(W / 2, 0, "NAHORE", fontSmall, YELLOW, BLACK);
  // barevné pruhy s popisem uvnitř (levý sloupec)
  const int bx = 8, bw = 156, bh = 30;
  int by = 34;
  fillRect(bx, by, bw, bh, RED);        drawTextCentered(bx + bw / 2, by + 3, "CERVENA", fontSmall, WHITE, RED);
  by += bh + 6;
  fillRect(bx, by, bw, bh, GREEN);      drawTextCentered(bx + bw / 2, by + 3, "ZELENA", fontSmall, BLACK, GREEN);
  by += bh + 6;
  fillRect(bx, by, bw, bh, BLUE);       drawTextCentered(bx + bw / 2, by + 3, "MODRA", fontSmall, WHITE, BLUE);
  by += bh + 10;
  snprintf(info, sizeof(info), "%02X i%d %s", MADCTL_OPTS[madIdx], invert, bgr ? "BGR" : "RGB");
  drawText(bx, by, info, fontSmall, CYAN, BLACK);
  drawText(bx, by + fontSmall.height + 4, "dotyk = tecka", fontSmall, GREY, BLACK);
}

// Velké číslo: naležato vpravo, nastojato dole uprostřed.
void drawDigit(const char *d) {
  int w = textWidth(fontBig, "8");
  int cx = landscape() ? 172 + (W - 172) / 2 : W / 2;
  int y  = landscape() ? (H - fontBig.height) / 2 + 4 : 200;
  fillRect(cx - w / 2 - 4, y, w + 8, fontBig.height, BLACK);
  drawText(cx - textWidth(fontBig, d) / 2, y, d, fontBig, GREEN, BLACK);
}

void help() {
  Serial.println(F("\n=== AW4 – test displeje RP2350-Touch-LCD-2 ==="));
  Serial.println(F(" o = dalsi natoceni   i = inverze   b = RGB/BGR   s = sken I2C   h = napoveda"));
  Serial.println(F(" Spravne: NAHORE nahore, rohy TL/TR/BL/BR sedi, barvy pruhu odpovidaji popisu, pozadi cerne."));
}

void report() {
  Serial.print(F("Nastaveni: MADCTL=0x")); Serial.print(MADCTL_OPTS[madIdx], HEX);
  Serial.print(F(" inverze=")); Serial.print(invert);
  Serial.print(F(" poradi=")); Serial.print(bgr ? F("BGR") : F("RGB"));
  Serial.print(F("  (")); Serial.print(W); Serial.print('x'); Serial.print(H); Serial.println(F(")"));
}

void setup() {
  Serial.begin(115200);
  uint32_t t = millis(); while (!Serial && millis() - t < 2500) {}
  pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);   // podsvícení
  SPI.setSCK(PIN_SCK); SPI.setTX(PIN_MOSI);
  SPI.begin();
  pinMode(PIN_DC, OUTPUT);   // GPIO16 je i výchozí MISO SPI0 – vrátit na obyčejný výstup
  lcdInit();
  drawScreen(); drawDigit("1");
  Wire.setSDA(PIN_SDA); Wire.setSCL(PIN_SCL); Wire.begin(); Wire.setClock(400000);
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  help(); report(); scanI2C();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    bool redraw = false;
    if (c == 'o' || c == 'O') { madIdx = (madIdx + 1) % sizeof(MADCTL_OPTS); redraw = true; }
    if (c == 'i' || c == 'I') { invert = !invert; redraw = true; }
    if (c == 'b' || c == 'B') { bgr = !bgr; redraw = true; }
    if (c == 's' || c == 'S') scanI2C();
    if (c == 'h' || c == 'H' || c == '?') help();
    if (redraw) { lcdApplyRotation(); drawScreen(); drawDigit("1"); report(); }
  }

  static uint32_t lastDigit = 0; static uint8_t d = 0;
  if (millis() - lastDigit > 1500) {
    lastDigit = millis();
    const char *seq[] = {"-", "1", "2", "3", "4"};
    d = (d + 1) % 5; drawDigit(seq[d]);
  }

  static uint32_t lastTouch = 0;
  if (millis() - lastTouch > 25) {
    lastTouch = millis();
    uint16_t tx, ty;
    if (readTouch(tx, ty)) {
      // surové souřadnice dotyku (nativně na výšku 240x320) – převod na obrazovku podle natočení
      uint16_t sx = tx, sy = ty;
      uint8_t m = MADCTL_OPTS[madIdx];
      if (m == 0x60) { sx = ty; sy = 239 - tx; }
      else if (m == 0xA0) { sx = 319 - ty; sy = tx; }
      else if (m == 0x20) { sx = ty; sy = tx; }
      else if (m == 0xE0) { sx = 319 - ty; sy = 239 - tx; }
      else if (m == 0xC0) { sx = 239 - tx; sy = 319 - ty; }
      fillRect(sx - 3, sy - 3, 7, 7, ORANGE);
      static uint32_t lastPrint = 0;
      if (millis() - lastPrint > 150) {
        lastPrint = millis();
        Serial.print(F("dotyk: surove x=")); Serial.print(tx); Serial.print(F(" y=")); Serial.print(ty);
        Serial.print(F("  -> obrazovka x=")); Serial.print(sx); Serial.print(F(" y=")); Serial.println(sy);
      }
    }
  }
}
