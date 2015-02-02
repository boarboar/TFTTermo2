
#include <Energia.h>
#include "hist.h"

// history
// 0 = head/latest -> TH_HIST_SZ-1 = tail/oldest
// 

uint8_t  TempHistory::interval_m(uint8_t  prev) {
  uint8_t ms=millis()/60000L;
  if(ms>=prev) return ms-prev;
  return (~prev)+ms;
}  

TempHistory::TempHistory() {
}

void TempHistory::init() {
  for(uint8_t i=0; i<TH_HIST_SZ; i++) { hist[i].mins=TH_EMPTY; hist[i].temp=0; hist[i].vcc=0; }
  acc_prev_time_m=millis()/60000L;
}

boolean TempHistory::addAcc(int16_t temp, int16_t vcc) {
  uint8_t i;
  acc.cnt++;
  acc.temp+=temp;
  acc.vcc+=vcc;
  uint8_t mins=interval_m(acc_prev_time_m); //time lapsed from previous storage
  if(mins>=TH_ACC_TIME) { 
    // add to hist 
    // compress first
    // test implementation...
    uint8_t cnt, mins_th;
    i=0; mins_th=mins; // start at head with 15 minutes   
    while(mins_th<122) {
      cnt=0;
      while(i<TH_HIST_SZ && hist[i].mins!=TH_EMPTY && hist[i].mins<mins_th+10) { i++; cnt++; }
      if(cnt>2) {
        i-=2; // step back for two positions
        hist[i].temp=(hist[i].temp+hist[i+1].temp)/2;
        hist[i].vcc=(hist[i].vcc+hist[i+1].vcc)/2;
        hist[i].mins=mins_th=hist[i].mins+hist[i+1].mins;
        for(cnt=i+1; cnt<TH_HIST_SZ-1; cnt++) hist[cnt]=hist[cnt+1]; // shift tail left // reuse cnt 
        hist[TH_HIST_SZ-1].mins=TH_EMPTY; //!!! now vacant
      } else break;
    } 
    for(i=TH_HIST_SZ-1; i>0; i--) hist[i]=hist[i-1]; // shift all right
    // add head
    hist[0].temp=(acc.temp/acc.cnt/TH_HIST_DV_T);
    hist[0].vcc=(acc.vcc/acc.cnt/TH_HIST_DV_V);
    hist[0].mins=mins;
   
    acc_prev_time_m=millis()/60000L;
    acc.temp=0;
    acc.vcc=0;
    acc.cnt=0;
    return true;
  } else return false;
}


int16_t TempHistory::getDiff(int16_t val, uint8_t sid) {
  if(hist[0].mins==TH_EMPTY) return 0;
  return val-(int16_t)hist[0].temp*TH_HIST_DV_T;
}

void TempHistory::iterBegin() { 
  iter_ptr=0xff;  
  iter_mbefore=interval_m(acc_prev_time_m); // time lapsed from latest storage
}

boolean TempHistory::movePrev() {
  iter_ptr++;
  if(iter_ptr>TH_HIST_SZ-1 || hist[iter_ptr].mins==TH_EMPTY) return false;
  iter_mbefore+=hist[iter_ptr].mins; // points to the moment iterated average started to accumulate 
  return true;
} 

uint8_t TempHistory::getSz() {
  uint8_t i=0;
  while(i<TH_HIST_SZ && hist[i].mins!=TH_EMPTY) i++;
  return i;
}
