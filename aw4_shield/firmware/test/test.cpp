#include "fw.cpp"
void run(int ms){ for(int i=0;i<ms;i++){ now++; loop(); } }
void press(int p){ pinv[p]=1; run(60); pinv[p]=0; run(60); }
void dump(const char*t){ printf("%-34s relay(D8)=%d  S1=%d S2=%d S3=%d  gear=%d  disp='%s'\n",t,pinv[8],pinv[9],pinv[10],pinv[11],gear, disp.msgs.empty()?"":disp.msgs[disp.msgs.size()-2].c_str()); }
int fails=0;
void expect(bool c,const char*m){ if(!c){printf("  FAIL: %s\n",m);fails++;} }
int main(){
  setup(); run(100); dump("start, READY off");
  expect(pinv[8]==1&&pinv[9]==0&&pinv[10]==0&&pinv[11]==0,"start: relay off, outputs off");
  log_.clear(); pinv[6]=1; run(60); dump("READY on");
  expect(pinv[8]==0,"READY: relay on"); expect(pinv[9]==1&&pinv[10]==0,"1st: S1 on S2 off");
  // order: solenoids before relay
  int iRel=-1,iS1=-1; for(size_t i=0;i<log_.size();i++){ if(log_[i]=="D8=0")iRel=i; if(log_[i]=="D9=1")iS1=i; }
  expect(iS1>=0 && iRel>iS1,"outputs set before relay pulls in");
  press(4); dump("UP -> 2"); expect(gear==2&&pinv[9]==1&&pinv[10]==1,"2nd: S1 on S2 on");
  pinv[7]=1; run(60); dump("LOCKUP switch on in 2nd"); expect(pinv[11]==0,"no lockup in 2nd");
  press(4); dump("UP -> 3"); expect(gear==3&&pinv[9]==0&&pinv[10]==1&&pinv[11]==1,"3rd: S1 off S2 on, lockup on");
  press(4); dump("UP -> 4"); expect(gear==4&&pinv[9]==0&&pinv[10]==0&&pinv[11]==1,"4th: both off, lockup");
  press(4); dump("UP at 4 (stays 4)"); expect(gear==4,"no gear 5");
  pinv[7]=0; run(60); dump("LOCKUP switch off"); expect(pinv[11]==0,"lockup off");
  press(5); press(5); press(5); press(5); dump("DOWN x4 -> 1"); expect(gear==1&&pinv[9]==1&&pinv[10]==0,"back to 1st");
  // bounce: 5 ms glitches must not shift
  for(int k=0;k<5;k++){ pinv[4]=1; run(5); pinv[4]=0; run(5);} run(50); dump("UP glitches 5ms"); expect(gear==1,"glitches ignored");
  log_.clear(); pinv[6]=0; run(60); dump("READY off");
  expect(pinv[8]==1&&pinv[9]==0&&pinv[10]==0&&pinv[11]==0,"READY off: relay off, outputs off");
  iRel=-1; int iOff=-1; for(size_t i=0;i<log_.size();i++){ if(log_[i]=="D8=1")iRel=i; if(log_[i]=="D9=0")iOff=i; }
  expect(iRel>=0 && iOff>iRel,"relay released before outputs drop");
  size_t n=disp.msgs.size(); run(2000); printf("display messages in 2 s idle: %zu\n", disp.msgs.size()-n);
  expect(disp.msgs.size()-n>=6 && disp.msgs.size()-n<=10,"periodic display refresh ~0.5 s");
  printf(fails? "\n%d FAILED\n":"\nALL CHECKS PASSED\n",fails); return fails;
}
