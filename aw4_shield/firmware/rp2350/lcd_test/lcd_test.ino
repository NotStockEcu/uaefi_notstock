// AW4 – test displeje a dotyku desky Waveshare RP2350-Touch-LCD-2 (na stole, jen přes USB).
//
// Arduino IDE: jádro "Raspberry Pi Pico/RP2040/RP2350" (Earle Philhower),
//   deska: Tools -> Board -> "Waveshare RP2350 ..." nebo "Raspberry Pi Pico 2".
// Žádné další knihovny nejsou potřeba – ovladač displeje (ST7789) a písma jsou v tomto sketchi.
//
// Serial Monitor 115200 Bd, příkazy (písmeno + Enter):
//   o = další natočení obrazu    i = inverze barev zap/vyp    b = pořadí barev RGB/BGR
//   s = znovu prohledat I2C      h = nápověda
// Cíl: nápis NAHORE je nahoře, rohy TL/TR/BL/BR sedí, pruhy mají barvy podle popisu
// (CERVENA je červená atd.) a pozadí je černé. Nastavení, které sedí, mi pošlete.
//
// Piny podle schématu RP2350-Touch-LCD-2:
//   LCD: BL=GPIO15, DC=16, CS=17, CLK=18, DIN=19, RST=20 (RST je společný i pro dotyk)
//   dotyk + IMU: I2C SDA=12, SCL=13, přerušení dotyku TP_INT=29
//
// Kód je v souboru lcd_test_main.cpp ve stejné složce.
