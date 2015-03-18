
#include <Energia.h>
#include "hist.h"

// history
// 0 = head/latest -> TH_HIST_SZ-1 = tail/oldest
// 

uint8_t  TempHistory::interval_m(uint8_t  prev) { //won't work if > 255 mins...
  uint8_t ms=millis()/60000L;
  if(ms>=prev) return ms-prev;
  return (~prev)+ms;
}  

TempHistory::TempHistory() {
}

void TempHistory::init() {
  uint8_t i;
  for(i=0; i<TH_HIST_SZ; i++) { TH_SETEMPTY(i); hist[i].temp=0; hist[i].vcc=0; }
  for(i=0; i<TH_SID_SZ; i++) acc_prev_time_m[i]=millis()/60000L;
}

boolean TempHistory::addAcc(int16_t temp, int16_t vcc, uint8_t sid) {
  //uint8_t i, cnt;
  uint8_t i, j;
  uint8_t ii[2];
  uint16_t mins_th, mins;
  if(sid>TH_SID_SZ) return false;
  mins=interval_m(acc_prev_time_m[sid-1]); //time lapsed from previous storage
  
  // compress first
  // test implementation...
  // sid IS IGNORED!!!
  i=0; mins_th=mins+10; // start at head with 15 minutes   
  while(mins_th<TH_ROLLUP_THR) {
    /*
    cnt=0;
    while(i<TH_HIST_SZ && !TH_ISEMPTY(i) && hist[i].mins<mins_th) { i++; cnt++; }
    */
    j=0; // as a counter
    ii[0]=ii[1]=0; // not necessary
    while(i<TH_HIST_SZ && !TH_ISEMPTY(i)) {
      if(hist[i].sid==sid && hist[i].mins<mins_th) {
        ii[0]=ii[1]; ii[1]=i;
        j++; 
       }
       i++;
    }
    if(j>2) {
      /*
        i-=2; // step back for two positions
        hist[i].temp=(hist[i].temp+hist[i+1].temp)/2;
        hist[i].vcc=(hist[i].vcc+hist[i+1].vcc)/2;
        mins_th=hist[i].mins=hist[i].mins+hist[i+1].mins;
        mins_th+=10;
        for(cnt=i+1; cnt<TH_HIST_SZ-1; cnt++) hist[cnt]=hist[cnt+1]; // shift tail left // reuse cnt 
        */
        i=ii[0];
        j=ii[1];
        hist[i].temp=(hist[i].temp+hist[j].temp)/2;
        hist[i].vcc=(hist[i].vcc+hist[j].vcc)/2;
        mins_th=hist[i].mins=hist[i].mins+hist[j].mins;
        mins_th+=10;
        for(; j<TH_HIST_SZ-1; j++) hist[j]=hist[j+1]; // shift tail left // reuse cnt 
        TH_SETEMPTY(TH_HIST_SZ-1); // now vacant
      } else break;
      
  } 
  for(i=TH_HIST_SZ-1; i>0; i--) hist[i]=hist[i-1]; // shift all right
  // add head
  hist[0].temp=temp/TH_HIST_DV_T;
  hist[0].vcc=vcc/TH_HIST_DV_V;
  hist[0].mins=mins;
  hist[0].sid=sid;
  acc_prev_time_m[sid-1]=millis()/60000L;
  return true;
}

TempHistory::wt_msg_hist *TempHistory::getData(uint8_t sid, uint8_t pos) {
  uint8_t i;
  pos++;
  for(i=0; i<TH_HIST_SZ && !TH_ISEMPTY(i); i++) {
    if(hist[i].sid==sid) {
      pos--; 
      if(!pos) break;
    }
  }
  if(pos) return NULL;
  else return hist+i;
}

uint8_t  TempHistory::getHeadDelay(uint8_t sid) { 
  sid--;
  if(sid>=TH_SID_SZ) return 0;
  return interval_m(acc_prev_time_m[sid]); 
}

// *** should be SID - specific!!!
void TempHistory::iterBegin() { 
  iter_ptr=0xff;  
  iter_mbefore=interval_m(acc_prev_time_m[0]); // time lapsed from latest storage
}

boolean TempHistory::movePrev() {
  iter_ptr++;
  if(iter_ptr>TH_HIST_SZ-1 || TH_ISEMPTY(iter_ptr)) return false;
  iter_mbefore+=hist[iter_ptr].mins; // points to the moment iterated average started to accumulate 
  return true;
} 
// ***

uint8_t TempHistory::getSz() {
  uint8_t i=0;
  while(i<TH_HIST_SZ && !TH_ISEMPTY(i)) i++;
  return i;
}

uint8_t TempHistory::check() {
  uint8_t i=0;
  while(i<TH_HIST_SZ && !TH_ISEMPTY(i)) i++;
  if(i==TH_HIST_SZ) return 0;
  while(i<TH_HIST_SZ && TH_ISEMPTY(i)) i++;
  return TH_HIST_SZ-i;
}
