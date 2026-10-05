// OBD2 ECU simulator for bench testing (displays, scan tools, rusEFI Lua / CAN dev).
// Arduino Micro + MCP2515 CAN module. ISO 15765-4, 11-bit ID, 500 kbps.
//
// Wiring (Arduino Micro -> MCP2515 module):
//   5V -> VCC, GND -> GND, ICSP SCK/MISO/MOSI -> SCK/SO/SI, D10 -> CS, D2 -> INT
// Library: "MCP_CAN" by coryjfowler (MCP_CAN_lib)
//
// Serial console (115200, newline terminated) changes the simulated values:
//   r <rpm>  s <km/h>  c <coolant C>  i <intake C>  t <throttle %>  l <load %>
//   m <0|1> MIL on/off   d <n> DTC count shown in PID 01   p  print state

#include <SPI.h>
#include <mcp_can.h>

const uint8_t PIN_CS = 10;
const uint8_t PIN_INT = 2;
const uint8_t CAN_CLOCK = MCP_16MHZ;  // change to MCP_8MHZ if your crystal is 8 MHz

const uint32_t ID_REQUEST_FUNCTIONAL = 0x7DF;
const uint32_t ID_REQUEST_ECU = 0x7E0;
const uint32_t ID_RESPONSE_ECU = 0x7E8;

// PIDs 01-20 supported: 01, 04, 05, 0C, 0D, 0F, 11, 1C
const uint32_t SUPPORTED_01_20 = 0x981A8010UL;

MCP_CAN can(PIN_CS);

uint16_t rpm = 850;
uint8_t speedKmh = 0;
int16_t coolantC = 90;
int16_t intakeC = 25;
uint8_t throttlePct = 0;
uint8_t loadPct = 20;
bool milOn = false;
uint8_t dtcCount = 0;

void printState() {
  Serial.print(F("rpm=")); Serial.print(rpm);
  Serial.print(F(" speed=")); Serial.print(speedKmh);
  Serial.print(F(" coolant=")); Serial.print(coolantC);
  Serial.print(F(" intake=")); Serial.print(intakeC);
  Serial.print(F(" throttle=")); Serial.print(throttlePct);
  Serial.print(F(" load=")); Serial.print(loadPct);
  Serial.print(F(" mil=")); Serial.print(milOn);
  Serial.print(F(" dtc=")); Serial.println(dtcCount);
}

void handleSerial() {
  static char line[24];
  static uint8_t n = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch != '\n' && ch != '\r') {
      if (n < sizeof(line) - 1) line[n++] = ch;
      continue;
    }
    line[n] = 0;
    n = 0;
    if (!line[0]) continue;
    long v = atol(line + 1);
    switch (line[0]) {
      case 'r': rpm = constrain(v, 0, 16383); break;
      case 's': speedKmh = constrain(v, 0, 255); break;
      case 'c': coolantC = constrain(v, -40, 215); break;
      case 'i': intakeC = constrain(v, -40, 215); break;
      case 't': throttlePct = constrain(v, 0, 100); break;
      case 'l': loadPct = constrain(v, 0, 100); break;
      case 'm': milOn = v != 0; break;
      case 'd': dtcCount = constrain(v, 0, 127); break;
      case 'p': break;
    }
    printState();
  }
}

void sendResponse(const uint8_t *payload, uint8_t len) {
  uint8_t buf[8] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
  buf[0] = len;
  memcpy(buf + 1, payload, len);
  can.sendMsgBuf(ID_RESPONSE_ECU, 0, 8, buf);
}

void handleMode01(uint8_t pid) {
  uint8_t r[8] = {0x41, pid};
  uint8_t len = 0;
  switch (pid) {
    case 0x00:
      r[2] = SUPPORTED_01_20 >> 24; r[3] = SUPPORTED_01_20 >> 16;
      r[4] = SUPPORTED_01_20 >> 8;  r[5] = SUPPORTED_01_20;
      len = 6; break;
    case 0x01:  // monitor status: MIL bit + DTC count, no monitors reported
      r[2] = (milOn ? 0x80 : 0) | (dtcCount & 0x7F); r[3] = 0; r[4] = 0; r[5] = 0;
      len = 6; break;
    case 0x04: r[2] = (uint16_t)loadPct * 255 / 100; len = 3; break;
    case 0x05: r[2] = coolantC + 40; len = 3; break;
    case 0x0C: {
      uint16_t raw = (uint32_t)rpm * 4;  // 16-bit int on AVR would overflow
      r[2] = raw >> 8; r[3] = raw & 0xFF; len = 4; break;
    }
    case 0x0D: r[2] = speedKmh; len = 3; break;
    case 0x0F: r[2] = intakeC + 40; len = 3; break;
    case 0x11: r[2] = (uint16_t)throttlePct * 255 / 100; len = 3; break;
    case 0x1C: r[2] = 6; len = 3; break;  // EOBD
    default: return;  // unsupported PID: stay silent like a real ECU
  }
  sendResponse(r, len);
}

void handleRequest(const uint8_t *d) {
  uint8_t len = d[0];
  if (len < 1 || len > 7) return;
  switch (d[1]) {
    case 0x01:
      if (len >= 2) handleMode01(d[2]);
      break;
    case 0x03: {  // stored DTCs: none
      const uint8_t r[] = {0x43, 0x00};
      sendResponse(r, 2);
      break;
    }
    case 0x04: {  // clear DTCs
      dtcCount = 0; milOn = false;
      const uint8_t r[] = {0x44};
      sendResponse(r, 1);
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_INT, INPUT);
  while (can.begin(MCP_STDEXT, CAN_500KBPS, CAN_CLOCK) != CAN_OK) {
    Serial.println(F("MCP2515 init failed, retrying"));
    delay(500);
  }
  can.init_Mask(0, 0, 0x7FF);
  can.init_Filt(0, 0, ID_REQUEST_FUNCTIONAL);
  can.init_Filt(1, 0, ID_REQUEST_ECU);
  can.init_Mask(1, 0, 0x7FF);
  can.init_Filt(2, 0, ID_REQUEST_FUNCTIONAL);
  can.init_Filt(3, 0, ID_REQUEST_ECU);
  can.init_Filt(4, 0, ID_REQUEST_ECU);
  can.init_Filt(5, 0, ID_REQUEST_ECU);
  can.setMode(MCP_NORMAL);
  Serial.println(F("OBD2 simulator ready"));
  printState();
}

void loop() {
  handleSerial();
  if (digitalRead(PIN_INT) == LOW) {
    uint32_t id;
    uint8_t len;
    uint8_t d[8];
    if (can.readMsgBuf(&id, &len, d) == CAN_OK && len >= 2) handleRequest(d);
  }
}
