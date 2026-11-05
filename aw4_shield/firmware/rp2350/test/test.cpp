#include "aw4_main.cpp"
int fails=0; void expect(bool c,const char*m){ if(!c){printf("  FAIL: %s\n",m);fails++;} }
void run(int ms){ for(int i=0;i<ms;i++){ nowMs++; loop(); } }
void press(int p){ gpio[p]=1; run(60); gpio[p]=0; run(60); }
void shot(const char*name){ FILE*f=fopen(name,"wb"); int W2=lcdEmu.LW(),H2=lcdEmu.LH(); fprintf(f,"P6 %d %d 255\n",W2,H2);
  for(int i=0;i<W2*H2;i++){ uint16_t c=lcdEmu.fb[i]; fputc(((c>>11)&31)*255/31,f); fputc(((c>>5)&63)*255/63,f); fputc((c&31)*255/31,f);} fclose(f); }
int main(){
  setup(); run(100);
  expect(lcdEmu.mad==0x00 && lcdEmu.inv,"portrait, inversion on");
  expect(gpio[PIN_RELAYS]==0 && gpio[PIN_SOL1]==0 && gpio[PIN_SOL2]==0 && gpio[PIN_SOL3]==0,"start: relays off, outputs off");
  shot("s0_tcu.ppm");
  press(PIN_UP); expect(gear==1,"no shift without READY");
  pinLog.clear(); gpio[PIN_READY]=1; run(60);
  expect(ready && gpio[PIN_RELAYS]==1 && gpio[PIN_SOL1]==1 && gpio[PIN_SOL2]==0,"READY: 1st, relays on");
  int iRel=-1,iS1=-1; for(size_t i=0;i<pinLog.size();i++){ if(pinLog[i]=="G21=1")iRel=i; if(pinLog[i]=="G23=1")iS1=i; }
  expect(iS1>=0 && iRel>iS1,"outputs set before relays pull in");
  shot("s1_gear1.ppm");
  press(PIN_UP); expect(gear==2 && gpio[PIN_SOL1]==1 && gpio[PIN_SOL2]==1,"2nd");
  gpio[PIN_LOCKUP]=1; run(60); expect(gpio[PIN_SOL3]==0,"no lockup in 2nd");
  press(PIN_UP); expect(gear==3 && gpio[PIN_SOL1]==0 && gpio[PIN_SOL2]==1 && gpio[PIN_SOL3]==1,"3rd + lockup");
  press(PIN_UP); expect(gear==4 && gpio[PIN_SOL1]==0 && gpio[PIN_SOL2]==0 && gpio[PIN_SOL3]==1,"4th + lockup");
  shot("s2_gear4_lockup.ppm");
  press(PIN_UP); expect(gear==4,"no 5th");
  gpio[PIN_LOCKUP]=0; run(60); expect(gpio[PIN_SOL3]==0,"lockup off");
  press(PIN_DOWN); press(PIN_DOWN); expect(gear==2,"down to 2");
  shot("s3_gear2.ppm");
  for(int k=0;k<5;k++){ gpio[PIN_DOWN]=1; run(5); gpio[PIN_DOWN]=0; run(5);} run(50); expect(gear==2,"5 ms glitches ignored");
  pinLog.clear(); gpio[PIN_READY]=0; run(60);
  expect(!ready && gpio[PIN_RELAYS]==0 && gpio[PIN_SOL1]==0 && gpio[PIN_SOL2]==0 && gpio[PIN_SOL3]==0,"READY off: relays released, outputs off");
  iRel=-1; int iOff=-1; for(size_t i=0;i<pinLog.size();i++){ if(pinLog[i]=="G21=0")iRel=i; if(pinLog[i]=="G23=0"||pinLog[i]=="G11=0")iOff=i; }
  expect(iRel>=0 && (iOff<0 || iOff>iRel),"relays released before outputs drop");
  printf("%s", Serial.out.c_str());
  printf(fails?"\n%d FAILED\n":"\nALL CHECKS PASSED\n",fails); return fails;
}
