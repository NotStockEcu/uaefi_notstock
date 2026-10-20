# NOT STOCK updater, ESP32-S3-Touch-AMOLED-1.75

The 1.85"'s Wi-Fi updater (`../notstock-updater185`, its `main.c` built
here with `UPD_SSID` NOTSTOCK-175, `UPD_GAUGE` notstock_round_amoled) on
the 1.75" AMOLED: `main/lcd_amoled.c` brings the CO5300 up as
`../notstock-round-amoled/main/hw_amoled.c` does and draws the updater's
lines (laid out for 360 rows, scaled to 466).

In the factory partition of `../notstock-round-amoled/partitions.csv`; the
gauge runs from ota_0. MENU -> AKTUALIZACE -> SPUSTIT in the gauge, then on
the phone: Wi-Fi NOTSTOCK-175, password notstock, http://192.168.4.1,
upload `notstock_round_amoled.bin`. As on the 1.85, see there.

    idf.py build      # build/notstock_updater175.bin, at 0x20000
