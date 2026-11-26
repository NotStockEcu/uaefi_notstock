#pragma once
#include <stddef.h>
#include <stdint.h>
// Náhrada I2C: dotyk se ovládá z testu přes touchSim
struct TouchSim { bool down=false; int x=0, y=0; } touchSim;
struct WireT {
  int reg=0, idx=0; uint8_t buf[5];
  void setSDA(int){} void setSCL(int){} void begin(){} void setClock(long){}
  void beginTransmission(int){} void write(int r){ reg=r; }
  int endTransmission(bool=true){ return 0; }
  size_t requestFrom(int, size_t n){ idx=0; buf[0]=touchSim.down; buf[1]=touchSim.x>>8; buf[2]=touchSim.x; buf[3]=touchSim.y>>8; buf[4]=touchSim.y; return n; }
  int read(){ return buf[idx++]; }
} Wire;
