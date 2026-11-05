#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>
#define PROGMEM
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define MSBFIRST 1
#define SPI_MODE0 0
struct Lcd { int dc=0; uint8_t cmd=0; int argn=0; uint8_t args[4]; int x0=0,x1=0,y0=0,y1=0,cx=0,cy=0; uint8_t mad=0; bool inv=false;
  uint16_t fb[320*320]; uint8_t hi; bool half=false;
  int LW(){return (mad&0x20)?320:240;} int LH(){return (mad&0x20)?240:320;}
  void pix(int x,int y,uint16_t c){ if(x<0||y<0||x>=LW()||y>=LH()) return; fb[y*LW()+x]=c; }
  void command(uint8_t c){ cmd=c; argn=0; half=false; if(c==0x21)inv=true; if(c==0x20)inv=false; if(c==0x2C){cx=x0;cy=y0;} }
  void data(uint8_t d){
    if(cmd==0x2A||cmd==0x2B){ args[argn++&3]=d; if(argn==4){int a=(args[0]<<8)|args[1], b=(args[2]<<8)|args[3]; if(cmd==0x2A){x0=a;x1=b;}else{y0=a;y1=b;}} }
    else if(cmd==0x36) mad=d;
    else if(cmd==0x2C){ if(!half){hi=d;half=true;} else {half=false; pix(cx,cy,(hi<<8)|d); if(++cx>x1){cx=x0;cy++;}} } }
  void byte(uint8_t b){ if(dc) data(b); else command(b); }
} lcdEmu;
static int gpio[32]; static int gpioMode[32]; static unsigned long nowMs=0; static std::vector<std::string> pinLog;
inline void pinMode(int p,int m){ gpioMode[p]=m; }
inline void digitalWrite(int p,int v){ if(p==16) lcdEmu.dc=v; if(gpioMode[p]==1 && gpio[p]!=(v?1:0)){ char b[24]; snprintf(b,24,"G%d=%d",p,v?1:0); pinLog.push_back(b);} gpio[p]=v?1:0; }
inline int digitalRead(int p){ return gpio[p]; }
inline void analogWriteRange(int){} inline void analogWrite(int,int){}
inline unsigned long millis(){ return nowMs; } inline void delay(unsigned long ms){ nowMs+=ms; }
struct SerialT{ std::string in; std::string out; void begin(long){} int available(){return (int)in.size();} int read(){char c=in[0]; in.erase(0,1); return c;}
  void println(const char*s){out+=s; out+="\n";} void print(const char*s){out+=s;}
  void printf(const char*f,...){ char b[256]; va_list a; va_start(a,f); vsnprintf(b,256,f,a); va_end(a); out+=b; } } Serial;
