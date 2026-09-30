# Bluetooth LE protocol

What the round gauge sends, for NOT STOCK Live (`../notstock-app`) or any
other client. Packed by `main/ble_proto.c` (no ESP-IDF in it); the NimBLE
service around it comes with the board bring-up.

The gauge advertises as `NOTSTOCK ...` with one primary service:

| | UUID | |
| --- | --- | --- |
| service | `6e6f7473-746f-636b-0000-000000000000` | |
| GAUGES | `...0001` | notify, 10 Hz, 16 bytes |
| DPF | `...0002` | notify, 1 Hz, 14 bytes |
| INFO | `...0003` | read, text `NOTSTOCK round <version>` |

Both frames fit the default 20-byte payload, so no MTU exchange is needed.
Little endian. A value that is not there (not read, no link) is `0x8000`.

Byte 1 of both frames is the flags: `0x01` data from the car, `0x02` the
particulate filter is regenerating, `0x04` night.

**GAUGES**

| byte | | scale |
| --- | --- | --- |
| 0 | version, 1 | |
| 1 | flags | |
| 2-3 | sequence, wraps | |
| 4-5 | water | int16, 0.1 °C |
| 6-7 | oil | int16, 0.1 °C |
| 8-9 | boost | int16, 0.01 bar |
| 10-11 | intake | int16, 0.1 °C |
| 12-13 | exhaust | int16, 1 °C |
| 14-15 | engine | uint16, 1 rpm |

**DPF**

| byte | | scale |
| --- | --- | --- |
| 0 | version, 1 | |
| 1 | flags | |
| 2-3 | soot, calculated | int16, 0.01 g |
| 4-5 | soot, measured | int16, 0.01 g |
| 6-7 | differential pressure | int16, 1 hPa |
| 8-9 | since regeneration | uint16, 0.1 km |
| 10-11 | filter temperature | int16, 1 °C |
| 12-13 | regenerations seen since power-up | uint16 |

Checked end to end: `ble_pack_*` output fed through the app's
`parseGauges` / `parseDpf` gives the values back (a missing value as
`null`).
