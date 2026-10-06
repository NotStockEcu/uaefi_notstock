#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
static int pinv[20]; static int pinMode_[20]; static uint32_t now=0;
static std::vector<std::string> log_;
inline void pinMode(int p,int m){pinMode_[p]=m;}
inline void digitalWrite(int p,int v){ if(pinMode_[p]==OUTPUT && pinv[p]!=(v?1:0)){ char b[32]; snprintf(b,32,"D%d=%d",p,v?1:0); log_.push_back(b);} pinv[p]=v?1:0;}
inline int digitalRead(int p){return pinv[p];}
inline uint32_t millis(){return now;}
struct SoftwareSerial{ SoftwareSerial(int,int){} void begin(long){} std::string cur; std::vector<std::string> msgs;
  void print(const char*s){cur+=s;} void write(uint8_t c){ if(c==0xFF){ if(!cur.empty()){msgs.push_back(cur);cur.clear();} } } };
