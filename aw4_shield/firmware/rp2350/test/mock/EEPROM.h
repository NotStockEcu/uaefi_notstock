#pragma once
#include <string.h>
#include <stdint.h>
struct EEPROMT { uint8_t mem[256]; int commits=0; EEPROMT(){ memset(mem,0xFF,sizeof mem);} void begin(int){}
  template<class T> void get(int a,T&v){ memcpy(&v,mem+a,sizeof v);} template<class T> void put(int a,const T&v){ memcpy(mem+a,&v,sizeof v);} bool commit(){ commits++; return true;} } EEPROM;
