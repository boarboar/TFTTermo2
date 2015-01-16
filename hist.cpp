
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
//  head_ptr=0; tail_ptr=TH_HIST_SZ-1;
//  acc={0, 0, 0, 0 };
//  since_1h_acc=0;
//  last_1h_acc_ptr=0;
 // since_3h_acc=0;
//  last_3h_acc_ptr=0;  
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
/*    
 //   hist[head_ptr].sid=1;
    hist[head_ptr].temp=(acc.temp/acc.cnt/TH_HIST_DV_T);
    hist[head_ptr].vcc=(acc.vcc/acc.cnt/TH_HIST_DV_V);
    hist[head_ptr].mins=mins;
    if(tail_ptr==head_ptr) tail_ptr = (tail_ptr+1)%TH_HIST_SZ;
    head_ptr = (head_ptr+1)%TH_HIST_SZ; // next (and the oldest reading)
    since_1h_acc+=mins;
    if(since_1h_acc>120) since_1h_acc=compress(1); // until last 2 hours
    
  //  since_3h_acc+=mins;
  //  if(since_3h_acc>1620) since_3h_acc=compress(3); // until last 27 hours (1620 mins)
  */
  
  /*
    // new compress pseudocode
  // test pass
    uint8_t cnt=0;
    i=0;
    //mins=mins;
    while(i<TH_HIST_SZ && hist[i].mins!=TH_EMPTY && hist[i].mins<mins+10) { i++; cnt++; }
    if(cnt>2) {
      i-=2;
      hist[i].temp=(hist[i].temp+hist[i+1].temp)/2;
      hist[i].vcc=(hist[i].vcc+hist[i+1].vcc)/2;
      hist[i].mins=hist[i].mins+hist[i+1].mins;
      for(i++; i<TH_HIST_SZ-1; i++) hist[i]=hist[i+1]; // shift left
    }
    */
    
    for(i=TH_HIST_SZ-1; i>0; i--) hist[i]=hist[i-1]; // shift right
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

/*
uint16_t TempHistory::compress(uint8_t level) {
   // hour accum here...
  // compress old records to hours...
    //int32_t acc_t=0;
    //int32_t acc_v=0;
    int16_t acc_t=0;
    int16_t acc_v=0;    
    uint8_t cnt=0;
    uint8_t iptr, tptr;
    uint16_t mins=0;
    uint16_t mins_limit=level==1 ? 60 : 180; 
    iptr=tptr = level==1 ? last_1h_acc_ptr : last_3h_acc_ptr;
    // calculate hour average
    do { 
      mins+=hist[iptr].mins;
      acc_t+=hist[iptr].temp;
      acc_v+=hist[iptr].vcc;
      cnt++;
      iptr = getNext(iptr);
    } while(iptr!=head_ptr && mins<mins_limit);
    // after loop exit, iptr->first item not accumulated 
    // store hour average
    hist[tptr].temp=acc_t/cnt;
    hist[tptr].vcc=acc_v/cnt;
    hist[tptr].mins=mins;
    
    tptr=getNext(tptr); 
    if(level==1) last_1h_acc_ptr=tptr; 
    else last_3h_acc_ptr=tptr; 
    
    // move items down... 
    mins=0;
    // before loop: iptr-> old position of first item not accumulated (before shift down)
    // before loop: tptr-> new position of first item not accumulated 
    // had_ptr -> old head (before shift down)
    while(iptr!=head_ptr) { 
      mins+=hist[iptr].mins;
      hist[tptr]=hist[iptr];
      if(level!=1 && iptr==last_1h_acc_ptr) last_1h_acc_ptr=tptr; // If we move after 3-hour cmpr, we ned to move down 1h ptr as well 
      iptr = getNext(iptr);
      tptr = getNext(tptr);
    }    
    head_ptr=tptr; 
    return mins;
}
*/

int16_t TempHistory::getDiff(int16_t val, uint8_t sid) {
  /*
  uint8_t lst=getPrev(head_ptr);      
  if(lst==tail_ptr) return 0;
  return val-(int16_t)hist[lst].temp*TH_HIST_DV_T;
  */
  if(hist[0].mins==TH_EMPTY) return 0;
  return val-(int16_t)hist[0].temp*TH_HIST_DV_T;
}

void TempHistory::iterBegin() { 
  //iter_ptr = head_ptr; 
  iter_ptr=0xff;  
  iter_mbefore=interval_m(acc_prev_time_m); // time lapsed from latest storage
}

boolean TempHistory::movePrev() {
  /*
  iter_ptr=getPrev(iter_ptr);   
  if(iter_ptr == tail_ptr) return false;
  iter_mbefore+=hist[iter_ptr].mins; // points to the moment iterated average started to accumulate 
  return true;
  */
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
