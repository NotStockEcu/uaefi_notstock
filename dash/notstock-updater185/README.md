# NOT STOCK updater, LCD 1.85"

A small app that puts a new round gauge firmware
([`../notstock-round-lcd185`](../notstock-round-lcd185)) on the Waveshare
ESP32-S3-Touch-LCD-1.85 over Wi-Fi, so the gauge need not come out of the
vent for it (the T5 housing keeps the display's USB-C inside).

It sits in the flash's `factory` partition, the gauge in `ota_0`
(`../notstock-round-lcd185/partitions.csv`):

- The gauge's MENU -> AKTUALIZACE -> SPUSTIT sets `upd` in NVS and boots
  this app.
- It opens an access point **NOTSTOCK-185**, password **notstock**, and a
  page at **http://192.168.4.1** (it answers every name a phone looks up,
  so the phone mostly offers the page itself as a sign-in page).
- The page takes `notstock_round_lcd185.bin`: it goes straight into
  `ota_0`, the first 4 kB checked first (an ESP32 app, and the gauge's,
  by its project name); the whole image is checked at the end; then the
  gauge boots. The display shows the network, the address and the
  progress.
- ZRUŠIT on the page, or 15 minutes without a request, boots the gauge as
  it was.
- An upload broken off half way leaves `ota_0` unusable and the boot choice
  here: the next start comes back here, to try again.
- Without `upd` (the first start after flashing over USB leaves the boot
  choice blank) it boots the gauge at once.

The panel is brought up as the gauge's `hw_lcd185.c` does (its driver and
pins, from `../notstock-round-lcd185/main`); text from `main/font.h`,
DejaVu Sans Bold at two sizes, made by `tools/gen_font.py`.

## Build and flash

ESP-IDF 5.x, from this folder: `idf.py build` (0.9 MB of its 1.9 MB).

The first time, over USB, both apps and the partition table with the
gauge's boot loader (from `../notstock-round-lcd185/build`):

```
esptool.py --chip esp32s3 -b 460800 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin \
  0x20000 notstock_updater185.bin 0x200000 notstock_round_lcd185.bin
```

Not `idf.py flash` in the gauge's folder: it puts the gauge at the first
app partition, this one's.
