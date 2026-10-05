# OBD2 simulator (Arduino Micro + MCP2515)

Bench-test tool: pretends to be an OBD2 ECU so you can develop and test
scan tools, displays and rusEFI Lua CAN code without a running engine.
It answers standard requests only; it is not meant to imitate any real vehicle's ECU.

## Hardware
- Arduino Micro (ATmega32U4 has no CAN) + MCP2515/TJA1050 module (5 V)
- SCK/MISO/MOSI from the **ICSP header**, CS = D10, INT = D2
- 120 ohm termination on the bus (most modules have a jumper)

## Software
- Arduino IDE, library **MCP_CAN** (coryjfowler)
- Open `obd2_sim/obd2_sim.ino`; set `CAN_CLOCK` to `MCP_8MHZ` if the module crystal is 8 MHz

## Supported
- 500 kbps, 11-bit, requests on `0x7DF` / `0x7E0`, replies on `0x7E8`, single frame
- Mode 01 PIDs: 00, 01, 04, 05, 0C, 0D, 0F, 11, 1C
- Mode 03 (no DTCs), Mode 04 (clear)
- Not implemented: multi-frame (VIN, Mode 09), 29-bit IDs, 250 kbps

## Serial console (115200, newline)
`r <rpm>` `s <km/h>` `c <coolant C>` `i <intake C>` `t <throttle %>` `l <load %>`
`m <0|1>` MIL, `d <n>` DTC count, `p` print state

Not compiled or tested on hardware yet.
