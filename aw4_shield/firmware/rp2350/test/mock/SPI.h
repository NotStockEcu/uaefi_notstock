#pragma once
#include "Arduino.h"
struct SPISettings{ SPISettings(uint32_t,uint8_t,uint8_t){} };
struct SPIClassMock{ bool setSCK(int){return true;} bool setTX(int){return true;} void begin(bool=false){}
 void beginTransaction(SPISettings){} void endTransaction(){} uint8_t transfer(uint8_t v){lcdEmu.byte(v); return v;}
 void transfer(const void*b,void*,size_t n){ const uint8_t*p=(const uint8_t*)b; for(size_t i=0;i<n;i++) lcdEmu.byte(p[i]); } } SPI;
