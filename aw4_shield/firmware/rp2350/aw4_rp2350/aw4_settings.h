// Nastavení uložené ve flash paměti modulu (EEPROM emulace jádra arduino-pico).
#pragma once
#include <Arduino.h>
#include <EEPROM.h>

struct Settings {
  uint32_t magic = 0xA4A40001;
  uint8_t lockMinGear = 3;     // lockup od stupně (3 nebo 4)
  uint8_t lockMinKmh = 45;     // lockup od rychlosti (jen s CANem)
  uint8_t lockMaxTps = 80;     // nad tímto plynem lockup rozepnout (jen s CANem)
  uint16_t downMaxRpm = 5000;  // zákaz podřazení nad těmito otáčkami po podřazení (jen s CANem)
  uint8_t bright = 100;        // jas podsvícení %
  uint8_t flip = 0;            // obraz otočený o 180°
  uint8_t canOn = 1;           // používat data z rusEFI
  uint8_t touchInv = 0;        // dotyk zrcadlený (kdyby nesouhlasil s obrazem)
  uint8_t sum = 0;
} cfg;

inline uint8_t cfgSum(const Settings &s) {
  const uint8_t *p = (const uint8_t *)&s; uint8_t c = 0x5A;
  for (size_t i = 0; i < offsetof(Settings, sum); i++) c = (c << 1 | c >> 7) ^ p[i];
  return c;
}
inline void cfgLoad() {
  EEPROM.begin(256);
  Settings s; EEPROM.get(0, s);
  if (s.magic == Settings().magic && s.sum == cfgSum(s)) cfg = s; else cfg = Settings();
}
inline void cfgSave() {
  cfg.sum = cfgSum(cfg);
  EEPROM.put(0, cfg); EEPROM.commit();
}
