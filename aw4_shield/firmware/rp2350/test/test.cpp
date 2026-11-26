#include "aw4_main.cpp"
int fails=0; void expect(bool c,const char*m){ if(!c){printf("  FAIL: %s\n",m);fails++;} }
// náhrada desky: čtení solenoidů = naše výstupy (relé přitažená) nebo stupeň „původní TCU“
int tcuSim=1; int brokenSns=-1; bool moving=false;
void board(){ for(int i=0;i<3;i++){ int v = gpio[PIN_RELAYS] ? gpio[i==0?PIN_SOL1:i==1?PIN_SOL2:PIN_SOL3]
                                          : (i<2 ? GEAR_TABLE[tcuSim-1][i] : 0);
                                   gpio[PIN_SNS[i]] = (i==brokenSns) ? 0 : v; } }
void run(int ms){ for(int i=0;i<ms;i++){ nowMs++; board(); if(moving && nowMs%20==0) vssIsr(); loop(); } }
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
  // brzda rozepne lockup
  press(PIN_UP); gpio[PIN_LOCKUP]=1; run(60); expect(gear==3 && gpio[PIN_SOL3]==1,"3rd + lockup again");
  gpio[PIN_BRAKE]=1; run(60); expect(gpio[PIN_SOL3]==0,"brake releases lockup");
  gpio[PIN_BRAKE]=0; run(60); expect(gpio[PIN_SOL3]==1,"lockup back after brake");
  // kontrola výstupu: přerušený vodič SOL2
  brokenSns=1; run(200); expect(solFault==2,"fault on S2 detected"); shot("s4_fault.ppm");
  brokenSns=-1; run(200); expect(solFault==0,"fault cleared");
  gpio[PIN_LOCKUP]=0; press(PIN_DOWN); run(60);
  pinLog.clear(); gpio[PIN_READY]=0; run(60);
  expect(!ready && gpio[PIN_RELAYS]==0 && gpio[PIN_SOL1]==0 && gpio[PIN_SOL2]==0 && gpio[PIN_SOL3]==0,"READY off: relays released, outputs off");
  iRel=-1; int iOff=-1; for(size_t i=0;i<pinLog.size();i++){ if(pinLog[i]=="G21=0")iRel=i; if(pinLog[i]=="G23=0"||pinLog[i]=="G11=0")iOff=i; }
  expect(iRel>=0 && (iOff<0 || iOff>iRel),"relays released before outputs drop");
  // READY za jízdy: převezme stupeň původní TCU
  tcuSim=3; moving=true; run(300); expect(!ready,"still TCU"); shot("s5_tcu3.ppm");
  gpio[PIN_READY]=1; run(60); expect(ready && gear==3 && gpio[PIN_SOL1]==0 && gpio[PIN_SOL2]==1,"take over TCU gear 3 while moving");
  gpio[PIN_READY]=0; run(60); moving=false; run(1500);
  tcuSim=3; gpio[PIN_READY]=1; run(60); expect(ready && gear==1,"standing still: start in 1st");
  // CAN z rusEFI: dekódování a pravidla
  { uint8_t f1[8]={0xC4,0x09,0,0,0,0,60,0};  ecuDecode(0x201,f1,8,nowMs); }   // 2500 ot/min, 60 km/h
  { uint8_t f2[8]={0,0,0xE8,0x03,0,0,0,0};   ecuDecode(0x202,f2,8,nowMs); }   // TPS 10 %
  { uint8_t f3[8]={0x10,0x0B,125,0,0,0,0,0}; ecuDecode(0x203,f3,8,nowMs); }   // CLT 85 C
  { uint8_t f4[8]={0,0,0,0,0,0,0x14,0x37};   ecuDecode(0x204,f4,8,nowMs); }   // 14.1 V
  expect(ecu.rpm==2500 && ecu.kph==60 && ecu.tps>9.9f && ecu.tps<10.1f && ecu.clt==85 && ecu.vbat>14.0f && ecu.vbat<14.2f,"rusEFI decode");
  expect(ecuValid(nowMs),"CAN valid");
  // udržovat data čerstvá během testu
  auto feed=[&](uint16_t rpm,uint8_t kph,float tps){ uint8_t a[8]={uint8_t(rpm),uint8_t(rpm>>8),0,0,0,0,kph,0}; ecuDecode(0x201,a,8,nowMs);
    int16_t t=int16_t(tps*100); uint8_t b[8]={0,0,uint8_t(t),uint8_t(t>>8),0,0,0,0}; ecuDecode(0x202,b,8,nowMs); };
  auto runF=[&](int ms,uint16_t rpm,uint8_t kph,float tps){ for(int i=0;i<ms;i++){ if(i%50==0) feed(rpm,kph,tps); nowMs++; board(); loop(); } };
  moving=false; tcuSim=1;
  feed(800,0,5); gpio[PIN_READY]=1; runF(60,800,0,5); expect(ready && gear==1,"READY with CAN standstill -> 1st");
  gpio[PIN_UP]=1; runF(60,800,0,5); gpio[PIN_UP]=0; runF(60,800,0,5);
  gpio[PIN_UP]=1; runF(60,800,0,5); gpio[PIN_UP]=0; runF(60,800,0,5); expect(gear==3,"up to 3rd");
  gpio[PIN_LOCKUP]=1; runF(100,1500,30,10); expect(gpio[PIN_SOL3]==0,"no lockup below 45 km/h");
  runF(100,2000,70,10); expect(gpio[PIN_SOL3]==1,"lockup at 70 km/h, low throttle");
  runF(100,2600,70,90); expect(gpio[PIN_SOL3]==0,"lockup released at 90 % throttle");
  // podřazení 3->2 při 3500 ot/min: 3500*1.53 = 5355 > 5000 -> zakázáno
  gpio[PIN_DOWN]=1; runF(60,3500,100,20); gpio[PIN_DOWN]=0; runF(60,3500,100,20); expect(gear==3,"downshift blocked by rpm");
  gpio[PIN_DOWN]=1; runF(60,2500,70,20); gpio[PIN_DOWN]=0; runF(60,2500,70,20); expect(gear==2,"downshift allowed at 2500 rpm");
  shot("s6_can.ppm");
  gpio[PIN_LOCKUP]=0; gpio[PIN_READY]=0; runF(60,800,0,5);
  nowMs+=1000; run(10); expect(!ecuValid(nowMs),"CAN timeout");
  // ---- dotykové menu ----
  auto tap=[&](int x,int y){ touchSim.x=x; touchSim.y=y; touchSim.down=true; run(40); touchSim.down=false; run(40); };
  expect(cfg.lockMinKmh==45 && cfg.bright==100,"default settings");
  tap(120,160); expect(menuOpen,"tap opens menu"); shot("s7_menu.ppm");
  tap(120,ROW_Y0+1*ROW_H+10); expect(menuSel==1,"select row 'Lockup od'");
  tap(200,290); tap(200,290); expect(cfg.lockMinKmh==55,"+ twice -> 55 km/h");
  tap(20,290); expect(cfg.lockMinKmh==50,"- -> 50 km/h");
  tap(120,ROW_Y0+3*ROW_H+10); tap(20,290); expect(cfg.downMaxRpm==4900,"max rpm 4900");
  int commits=EEPROM.commits; tap(120,290); expect(!menuOpen && EEPROM.commits==commits+1,"ZPET closes and saves");
  { Settings saved=cfg; cfg=Settings(); cfgLoad(); expect(cfg.lockMinKmh==50 && cfg.downMaxRpm==4900 && saved.sum==cfg.sum,"settings reload from flash"); }
  tap(120,160); expect(menuOpen,"menu again"); nowMs+=MENU_TIMEOUT_MS+10; run(30); expect(!menuOpen,"menu auto-close");
  tap(120,160); gpio[PIN_READY]=1; run(60); expect(!menuOpen && ready,"READY closes menu"); gpio[PIN_READY]=0; run(60);
  shot("s8_after_menu.ppm");
  printf("%s", Serial.out.c_str());
  printf(fails?"\n%d FAILED\n":"\nALL CHECKS PASSED\n",fails); return fails;
}
