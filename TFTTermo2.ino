#include <SPI.h>
#include "NRF24.h"
#include "RTClibSS.h"
#include "TFT_ILI9341.h"
#include "hist.h"

//TODO 

// 5523 PINOUT

// SCK  P1_5
// MOSI P1_7
// MISO P1_6

//#define TFT_CS_PIN P2_0
//#define TFT_DC_PIN P2_5 

#define TFT_BL_PIN P1_4 
  
//#define DS1302_SCLK_PIN   P2_4    
//#define DS1302_IO_PIN     P2_5    
//#define DS1302_CE_PIN     P2_2   


//#define NRF_CE_PIN P2_3
//#define NRF_SS_CSN_PIN P2_1
#define NRF_IRQ_PIN P1_0

#define BUTTON_1  P1_3
#define BUTTON_2  P2_6

#define WS_CHAN  1
#define WS_MSG_TEMP  0xFE

#define WS_ALR_WFAIL  0x1
#define WS_ALR_BADMSG 0x2
#define WS_ALR_BAD_TEMP 0x4
#define WS_ALR_TO     0x8
#define WS_ALR_LOWVCC 0x16
#define WS_ALR_BAD_LENGTH 0x32

#define WS_ALR_WFAIL_IDX  0
#define WS_ALR_BADMSG_IDX 1
#define WS_ALR_BAD_TEMP_IDX 2
#define WS_ALR_TO_IDX     3
#define WS_ALR_LOWVCC_IDX 4
#define WS_ALR_BAD_LENGTH_IDX 5
#define WS_ALR_LAST_IDX 5

//#define WS_SENS_TIMEOUT_M 30 // minutes

#define DS18_MEAS_FAIL	9999  // Temporarily!
#define DS18_MEAS_NONE  -999

// flags - only high bits, low bits reserved for value type

#define WS_FLAG_RFREAD    0x10
#define WS_FLAG_ONCEFAIL  0x20
#define WS_FLAG_ONDATAUPDATE  0x40
#define WS_FLAG_NEEDUPDATE  0x80

#define WS_NUILEV 7
#define WS_UI_MAIN  0
#define WS_UI_STAT   1
#define WS_UI_SET   6
#define WS_UI_CHART 3
#define WS_UI_CHART60 4
#define WS_UI_CHART_VCC 5
#define WS_UI_HIST  2

#define WS_CHAR_UP 128
#define WS_CHAR_DN 129
#define WS_CHAR_TIME_SZ  4
#define WS_CHAR_TEMP_SZ  6
#define WS_CHAR_TIME_SET_SZ  6
#define WS_CHAR_DEF_SIZE 2

#define WS_SCREEN_SIZE_X  320
#define WS_SCREEN_SIZE_Y  240
#define WS_SCREEN_STAT_LINE_Y  224
#define WS_SCREEN_TIME_LINE_Y  0
#define WS_SCREEN_TEMP_LINE_Y  60
#define WS_SCREEN_TEMP_LINE_PADDING 8
#define WS_SCREEN_TITLE_X (WS_SCREEN_SIZE_X-4*WS_CHAR_DEF_SIZE*FONT_SPACE)

#define WS_UI_CYCLE 50 
#define WS_DISP_CNT    10  // in UI_CYCLEs (=0.5s)
#define WS_INACT_CNT   40  // in DISP_CYCLEs (= 20s)

#define WS_CHART_NLEV 4

#define WS_BUT_NONE  0x00
#define WS_BUT_PRES  0x01

#define WS_BUT_MAX   13 
#define WS_BUT_CLICK 14 
#define WS_BUT_LONG 15

#define WS_TIMESET_DIG_T 4
#define WS_TIMESET_DIG_D 6
//#define WS_TIMESET_DIG 10

#define CHART_WIDTH 240
#define CHART_HEIGHT 204
#define CHART_TOP 19

#define GETHBHI(B) ((B)>>4)
#define GETHBLO(B) ((B)&0x0f)
#define SETHBHI(B, H) (B=(B&0x0f)+((H)<<4))
#define SETHBLO(B, L) (B=(B&0xf0)+((L)&0x0f))

#define SETCHRT(T)   SETHBLO(flags, (T))
#define GETCHRT()   GETHBLO(flags)

#define printVal(T, V) ((T)==TH_HIST_VAL_T ? printTemp(V) : printVcc(V))

#define TIME_SET_MODE() (uilev==WS_UI_SET && pageidx>0)

const char *addr="wserv";

const char *lnames[] = {"", "Stat", "Hist", "Temp", "Day", "Vcc", "Set"};
const char *dummy_t = " --.-";
const char *dummy_v = "-.--";

static const uint16_t cc[TH_SID_SZ]={CYAN, YELLOW};  

struct wt_msg {
  uint8_t msgt;
  uint8_t sid;
  int16_t temp;
  int16_t vcc;
}; 

TempHistory mHist;

// state vars

// ************************ UI

uint32_t mui;
//uint32_t rts=0; // this can be rid off later
uint16_t _lp_vpos=0; // make 8bit, and prop to char size?
uint16_t _lp_hpos=0;  // make 8bit, and prop to char size?

uint16_t msgcnt=0; // this can be rid off later

// trancient vars

union /*__attribute__((__packed__))*/ {
  char buf[6];
  struct { int16_t xr, x1, y1; } CV;
  struct { int16_t acc, h, y0; } HV;
  struct { int16_t ig; } CPV;
  struct { unsigned long ms; } MLV;
} _S;

union /*__attribute__((__packed__))*/ {
  struct { int16_t mint, maxt; } CMM; // this is for charting
  struct { uint8_t p_time[2], p_to[2]; } TT; // main screen time and timeout display
  struct { uint8_t dt[3]; uint8_t dtf;} TS; // time set
} _S2;

volatile uint8_t flags=0; // flgs in HI half, chart type encoded in LO half

uint8_t alarms=0;

// ************************ UI

uint8_t inact_cnt, disp_cnt=0;  
uint8_t btcnt1=0, btcnt2=0;
uint8_t uilev=WS_UI_MAIN;   // compress ?

uint8_t pageidx=0; // compress ? 

TFT Tft;
NRF24 nrf24;
RTC_DS1302 RTC;

void setup()
{
  delay(100);
   // disable XTAL
  P2SEL &= ~BIT6; 
  P2SEL &= ~BIT7;
 
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
    
  Tft.TFTinit(/*TFT_CS_PIN, TFT_DC_PIN*/);
  Tft.setOrientation(LCD_LANDSCAPE);
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
  
  lcd_defaults();

  mHist.init(); 
  
  /*
  line_init();
  line_printn("built: "); line_print(__DATE__);
  */
  
  dispStat("INIT CLK"); delay(200);  
  RTC.begin();
  
  dispStat("INIT NRF"); delay(200);
  uint8_t err=radioSetup();
  pinMode(NRF_IRQ_PIN, INPUT);  
  attachInterrupt(NRF_IRQ_PIN, radioIRQ, FALLING); 
  
  if(!err) dispStat("INIT OK.");  
  else dispErr(err);
  delay(200);

  mui=millis();
  
  //rts = RTC.now().unixtime();
  updateScreen();
}

void loop()
{
  flags &= ~WS_FLAG_NEEDUPDATE;

  if(flags&WS_FLAG_RFREAD) {
   flags &= ~WS_FLAG_RFREAD;
   uint8_t err=radioRead();  
   if(err) dispErr(err);
   else 
     flags |= WS_FLAG_ONDATAUPDATE;
  }

  _S.MLV.ms=millis(); 
  if(_S.MLV.ms-mui>WS_UI_CYCLE || _S.MLV.ms<mui) { // UI cycle
   mui=_S.MLV.ms;
   if(uilev!=WS_UI_MAIN && !TIME_SET_MODE() && !inact_cnt) {  // back to main screen after user inactivity timeout
     uilev=WS_UI_MAIN;
     flags|=WS_FLAG_NEEDUPDATE;
   }

   btcnt1=processClick(BUTTON_1, btcnt1);
   if(btcnt1>WS_BUT_MAX) {
     if(btcnt1==WS_BUT_CLICK) processShortClick(); 
     else processLongClick();
     btcnt1=0;     
   }
   
   btcnt2=processClick(BUTTON_2, btcnt2);
   if(btcnt2>WS_BUT_MAX) {
     if(btcnt2==WS_BUT_CLICK) processShortRightClick(); 
     btcnt2=0;     
   }
   if(TIME_SET_MODE()) { // in edit mode
     if(flags&WS_FLAG_NEEDUPDATE)
       updateScreen();     
   } 
   else { // not in edit mode
     if(flags&WS_FLAG_ONDATAUPDATE)
       dispStat("READ ");
     if(flags&(WS_FLAG_NEEDUPDATE|WS_FLAG_ONDATAUPDATE))
       updateScreen();
     
     if(++disp_cnt>=WS_DISP_CNT) { // 0.5 sec screen update   
       disp_cnt=0;   
       if(inact_cnt) inact_cnt--;
       /*
       if(!(alarms&WS_ALR_TO) && mHist.getHeadDelay(1)>WS_SENS_TIMEOUT_M) { // Alarm condition on no-data timeout, at the moment for SID1 only....           
         alarms |= WS_ALR_TO;
         if(uilev==WS_UI_MAIN) updateScreen();       
       }
       */
       if(mHist.timeout() && uilev==WS_UI_MAIN) updateScreen();
       if(uilev==WS_UI_MAIN) updateScreenTime(false);       
     }     
     
   }
 } // UI cycle
 
}

int8_t processClick(uint8_t id, uint8_t cnt) {
   if(digitalRead(id)==LOW) {
     if(!cnt) cnt=1;
     else if(cnt<WS_BUT_MAX) cnt++;
   } 
   else if(cnt) {
     if(cnt<2) {;} // debounce
     else {
        inact_cnt=WS_INACT_CNT;
        if(cnt<WS_BUT_MAX) return WS_BUT_CLICK;
        else return WS_BUT_LONG;        
      }
     cnt=0;
   }
  return cnt;
}

void processShortClick() {
  if(TIME_SET_MODE()) { // in edit mode - move to next digit
    timeEditMove();
  }
  else { // move to next screen
    uilev=(uilev+1)%WS_NUILEV;
    pageidx=0; 
    flags|=WS_FLAG_NEEDUPDATE;
  }
}

void processLongClick() {
  if(uilev==WS_UI_SET) {
    if(!pageidx) {
      timeEditOn();
    } else {
      timeStore();
    }
  } 
}

void processShortRightClick() {
  lcd_defaults();
  line_init();
  switch(uilev) {
    case WS_UI_CHART: 
    case WS_UI_CHART_VCC: 
      if(++pageidx>=WS_CHART_NLEV) pageidx=0;
      flags|=WS_FLAG_NEEDUPDATE;
      break;
    case WS_UI_HIST:
      flags|=WS_FLAG_NEEDUPDATE;
      break;
    case WS_UI_CHART60:  
      if(++pageidx>=TH_SID_SZ) pageidx=0;
      flags|=WS_FLAG_NEEDUPDATE;
      break;
    case WS_UI_SET:
      if(pageidx) {
        timeUp(pageidx-1, WS_CHAR_TIME_SET_SZ);
      }   
     break;
   default:break;  
  }
}       

void updateScreen() {
  lcd_defaults();
  line_init();

  if(!(flags&WS_FLAG_ONDATAUPDATE)) {
    Tft.fillScreen();
    Tft.drawString(lnames[uilev], WS_SCREEN_TITLE_X, 0);
  }
  
  switch(uilev) {
    case WS_UI_MAIN: {           
     if(flags&WS_FLAG_ONDATAUPDATE && mHist.getLatestSid()) dispMain(mHist.getLatestSid());
     else {
       for(uint8_t i=1; i<=TH_SID_SZ; i++) dispMain(i);
       updateScreenTime(true);   
      }
     }      
     break;
    case WS_UI_HIST: 
      pageidx=printHist(0xFF, pageidx);
      break;    
    case WS_UI_CHART: {
      SETCHRT(TH_HIST_VAL_T);
      chartHist();
      }
      break; 
    case WS_UI_CHART60: 
      chartHist60();
      break;
    case WS_UI_CHART_VCC: {
      SETCHRT(TH_HIST_VAL_V);
      chartHist();
      }
      break;     
    case WS_UI_STAT: 
      printStat();    
      break;
    case WS_UI_SET: 
      dispSetTime();
      break;
    default: break;
  }
  flags &= ~WS_FLAG_ONDATAUPDATE;
}

void dispMain(uint8_t sid) {
  TempHistory::wt_msg_hist *l=mHist.getData(sid, 0);  
  TempHistory::wt_msg_hist *p=mHist.getData(sid, 1);  
  uint16_t y=WS_SCREEN_TEMP_LINE_Y+(FONT_Y*WS_CHAR_TEMP_SZ+WS_SCREEN_TEMP_LINE_PADDING)*(sid-1);
  if(!(flags&WS_FLAG_ONDATAUPDATE) || !l || !p || (l->getVal(TH_HIST_VAL_T)!=p->getVal(TH_HIST_VAL_T))) {  
    int16_t t;
    //Tft.setColor(mHist.getHeadDelay(sid)>WS_SENS_TIMEOUT_M ? RED : GREEN);
    Tft.setColor(GREEN);
    Tft.setSize(WS_CHAR_TEMP_SZ);
    t=Tft.drawString(l ? printTemp(l->getVal(TH_HIST_VAL_T)) : dummy_t, 0, y);
    Tft.drawString("  ", t, y); // clear space
    Tft.setSize(WS_CHAR_TEMP_SZ/2);
    t=Tft.drawString("o", t, y); // grad
    Tft.drawString("C", t, y+FONT_Y*WS_CHAR_TEMP_SZ/2);
    Tft.setColor(YELLOW);
    if(!l || !p) t=0;
    else t=l->getVal(TH_HIST_VAL_T)-p->getVal(TH_HIST_VAL_T);
    Tft.drawCharLowRAM(t==0? ' ': t>0 ? WS_CHAR_UP : WS_CHAR_DN, FONT_SPACE*WS_CHAR_TEMP_SZ*6, y);    
    lcd_defaults();
  }
  if(!(flags&WS_FLAG_ONDATAUPDATE) || !l || !p || (l->getVal(TH_HIST_VAL_V)!=p->getVal(TH_HIST_VAL_V))) {
    line_setpos(WS_SCREEN_SIZE_X-WS_CHAR_DEF_SIZE*FONT_SPACE*5, y); 
    line_printn(l ? printVcc(l->getVal(TH_HIST_VAL_V)) : dummy_v); 
    line_printn("v");
  }     
}

void updateScreenTime(bool reset) {
  if(uilev!=WS_UI_MAIN) return;
  DateTime now = RTC.now();
  if(reset) { _S2.TT.p_time[0]=_S2.TT.p_time[1]=0xFF;}
  for(uint8_t i=1; i<=TH_SID_SZ; i++) dispTimeoutTempM(i, reset);

  if(reset || now.hour()!=_S2.TT.p_time[0]) {
      Tft.setSize(WS_CHAR_TIME_SZ/2);
      Tft.setColor(YELLOW);
      line_setpos(WS_CHAR_TIME_SZ*FONT_SPACE*8, 0);
      printDate(now);
  }            
  
  if(now.hour()!=_S2.TT.p_time[0] || now.minute()!=_S2.TT.p_time[1]) {
      printTime2(now, 0, WS_SCREEN_TIME_LINE_Y, WS_CHAR_TIME_SZ);   
      _S2.TT.p_time[0]=now.hour(); _S2.TT.p_time[1]=now.minute();
  }        
}

// buf 5
char *printTemp(int16_t disptemp) {
  _S.buf[0]='+';
  if(disptemp<0) {
    _S.buf[0]='-';
    disptemp=-disptemp;
  } else if(disptemp==0) _S.buf[0]=' ';
  itoa((uint8_t)(disptemp/10), _S.buf+1, 10);
  strcat(_S.buf, ".");
  itoa((uint8_t)(disptemp%10), _S.buf+strlen(_S.buf), 10);
  return _S.buf;
} 

// buf 4
char *printVcc(int16_t vcc) {
  itoa((uint8_t)(vcc/100), _S.buf, 10);
  strcat(_S.buf, ".");
  if(vcc%100<10) strcat(_S.buf, "0");
  itoa((uint8_t)(vcc%100), _S.buf+strlen(_S.buf), 10);
  return _S.buf;
}

void dispTimeoutTempM(uint8_t sid, bool reset) {
  line_setpos(WS_SCREEN_SIZE_X-WS_CHAR_DEF_SIZE*FONT_SPACE*5, WS_SCREEN_TEMP_LINE_Y+(FONT_Y*WS_CHAR_TEMP_SZ+WS_SCREEN_TEMP_LINE_PADDING)*(sid-1)+FONT_Y*WS_CHAR_TEMP_SZ/2);
  uint8_t tm=mHist.getHeadDelay(sid);
  if(reset) _S2.TT.p_to[sid-1]=0xFF;
  if(tm != _S2.TT.p_to[sid-1]/* || reset*/) {
    uint8_t hm=tm/60;
    if(hm>0) {
      line_printn(">"); line_printn(itoas2(hm)); line_printn(" H");    
    }
    else {
      hm=tm%60;
      line_printn(itoas2(hm)); line_printn(":00");
    }
    _S2.TT.p_to[sid-1]=tm;
  }
}

void dispTimeoutStatic(uint32_t ts) {
  uint8_t days=ts/3600/24;  
  if(days>0) {
    line_printn("> "); line_printn(itoas(days)); line_printn(" days");    
  }
  else {
     line_printn(itoas2((ts/3600)%24)); line_printn(":"); line_printn(itoas2((ts/60)%60)); line_printn(":"); line_printn(itoas2(ts%60));
  }     
}

void printTime2(const DateTime& pDT, int x, int y, int sz){
  uint8_t h=pDT.hour(), m=pDT.minute(); 
  Tft.setSize(sz);
  Tft.setColor(YELLOW);
  line_setpos(x, y);
  line_printn(itoas2(h)); line_printn(":"); line_printn(itoas2(m));
  lcd_defaults();
}

void printDate(const DateTime& pDT) {
 line_printn(itoas2(pDT.day())); line_printn("/"); line_printn(itoas2(pDT.month())); line_printn("/"); line_printn(itoas2(pDT.year()));
}

void dispSetTime() {
  DateTime now = RTC.now();
  if(pageidx==0 || _S2.TS.dtf==0) { // idle or edit time
    printTime2(now, 0, WS_SCREEN_TIME_LINE_Y, WS_CHAR_TIME_SET_SZ);   
  }
  if(pageidx==0 || _S2.TS.dtf==1) {
    Tft.setSize(WS_CHAR_TIME_SET_SZ);
    Tft.setColor(YELLOW);
    line_setpos(0, pageidx==0 ? WS_CHAR_TIME_SET_SZ*FONT_Y : 0);
    printDate(now);
  } 
  if(pageidx) hiLightDigit(WHITE); 
}

void timeEditOn() {
  pageidx=1;
  DateTime now=RTC.now();
  _S2.TS.dt[0]=now.hour(); _S2.TS.dt[1]=now.minute(); 
  _S2.TS.dtf=0; // time edit
  dispStat("EDT T ON");
  flags|=WS_FLAG_NEEDUPDATE;
}

void timeEditMove() {
  dispStat("EDIT MOV");
  hiLightDigit(BLACK);
  ++pageidx;
  if(_S2.TS.dtf==0) { if(pageidx>WS_TIMESET_DIG_T) pageidx=1; }
  else { if(pageidx>WS_TIMESET_DIG_D) pageidx=1; }
  hiLightDigit(WHITE);
}

void timeUp(uint8_t dig, int sz) {
  const uint8_t maxv_t[2]={24, 60};
  const uint8_t maxv_d[3]={32, 13, 32};

  //dig=0..3
  //hhmm
  //0123
  //ddmmyy
  //012345
  
  if(dig>5) return;
  uint8_t ig=dig/2; // 0-h, 1-m ; 0-d, 1-m, 2-y
  uint8_t id=(dig+1)%2; // 1-high dec, 0 - low dec 
  uint8_t val=_S2.TS.dt[ig];
  uint8_t pos=ig*3; uint8_t disp=0;
  const uint8_t *maxv=_S2.TS.dtf==0 ? maxv_t : maxv_d;
  if(id) { val+=10; if(val>maxv[ig]) val=val%10; disp=val/10;}  
  else { val=(val/10)*10+((val%10)+1)%10; if(val>maxv[ig]) val=(val/10)*10;  pos++; disp=val%10;} 
  Tft.setSize(sz);
  Tft.setColor(GREEN);
  Tft.drawCharLowRAM('0'+disp,sz*FONT_SPACE*pos, WS_SCREEN_TIME_LINE_Y);
  _S2.TS.dt[ig]=val; 
  hiLightDigit(WHITE);
}

void timeStore() {
  DateTime set=RTC.now();
  if(_S2.TS.dtf==0) { // time store
    set.setTime(_S2.TS.dt[0], _S2.TS.dt[1], 0);
    RTC.adjust(set); 
    //hiLightDigit(BLACK);
    pageidx=1;
    _S2.TS.dtf=1; // move to date
    _S2.TS.dt[0]=set.day(); _S2.TS.dt[1]=set.month(); _S2.TS.dt[2]=set.year();
    dispStat("TIME STR");
  } else {
    pageidx=0;
    set.setDate(_S2.TS.dt[0], _S2.TS.dt[1], _S2.TS.dt[2]);
    RTC.adjust(set); 
    dispStat("DATE STR");
  }
  delay(500);
  flags|=WS_FLAG_NEEDUPDATE;
}

void hiLightDigit(uint16_t color) {
  uint8_t x=pageidx;
  uint8_t y=0;
  uint8_t d=(x>0 && x<3)? x-1 :
            (x>4 ) ? x+1 :x;
  Tft.setColor(color);
  Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*d, WS_SCREEN_TIME_LINE_Y+y*WS_CHAR_TIME_SET_SZ*FONT_Y, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ);  
}

void dispErr(uint8_t err) {
  lcd_defaults();
  Tft.setColor(RED);
  line_setpos(0, WS_SCREEN_STAT_LINE_Y); line_printn("ERR="); line_printn(itoas(err));
  Tft.setColor(GREEN);
}

void dispStat(const char *pbuf) {
  lcd_defaults();
  line_setpos(0, WS_SCREEN_STAT_LINE_Y); line_printn(pbuf); 
  if(mHist.getLatestSid()!=0xF)
    line_printn(itoas(mHist.getLatestSid()));
}


/****************** REPORTS ****************/

void printStat() {
   DateTime now=RTC.now(); 
   uint8_t i;
   line_printn("NOW: "); printDate(now); line_printn(", "); line_print(now.dayOfWeekStr());
   line_printn("UPT: "); dispTimeoutStatic(millis()/1000); line_print("");
   //line_printn("RTT: "); dispTimeoutStatic(now.unixtime()-rts); line_print("");
   for(i=1; i<=TH_SID_SZ; i++) { // i for sid
     mHist.iterBegin(i);  
     while(mHist.movePrev());
     line_printn("DUR"); line_printn(itoas(i)); line_printn(": "); dispTimeoutStatic((uint32_t)mHist.getPrevMinsBefore()*60); line_print("");      
   }
   line_printn("CNT="); line_printn(itoa(msgcnt, _S.buf, 10)); line_printn(" HSZ="); line_print(itoas(mHist.getSz()));
   line_printn("CHK="); line_print(itoas(mHist.check()));  
   for(i=0; i<=WS_ALR_LAST_IDX; i++) {
     line_printn("A");line_printn(itoas(i));line_printn("=");
     if(alarms&(1<<i)) line_print("Y");
     else line_print("N");
   }  
}   
       
uint8_t printHist(uint8_t sid, uint8_t idx) {
  if(!startIter(sid)) return 0; 
  uint8_t i=0;
  while(i<idx && mHist.movePrev()) i++;
  if(i<idx) { // wraparound
    mHist.iterBegin(sid);
    mHist.movePrev();
    i=0;
  }  
  do {
    TempHistory::wt_msg_hist *h = mHist.getPrev();
    line_printn(itoas(i)); 
    line_setcharpos(4);
    line_printn(itoas(h->sid)); 
    line_setcharpos(6);
    line_printn(h->mins ? printTemp(h->getVal(TH_HIST_VAL_T)) : dummy_t); 
    line_setcharpos(13);
    line_printn(h->mins ? printVcc(h->getVal(TH_HIST_VAL_V)) : dummy_v); 
    line_setcharpos(18);
    line_print(itoa(h->mins ? h->mins : TH_GAP, _S.buf, 10));
    i++;    
    } while(mHist.movePrev() && line_getpos()<230);
  return i;  
}

void chartHist() {    
  const uint8_t chart_xstep_denoms[WS_CHART_NLEV]={7, 21, 49, 217};
  uint8_t chart_xstep_denom = chart_xstep_denoms[pageidx];
  uint8_t i;

  prepChart(0xFF, GETCHRT(), (uint16_t)CHART_WIDTH*chart_xstep_denom+60);
  if(_S2.CMM.maxt==_S2.CMM.mint) return;
  
  { // ++scope 1
  int16_t x0, y0;  
  _S.CV.xr=0;
  for(i=1; i<=TH_SID_SZ; i++) { // i for sid
     mHist.iterBegin(i);  
     while(mHist.movePrev());
     if(mHist.getPrevMinsBefore()>_S.CV.xr) _S.CV.xr=mHist.getPrevMinsBefore();
   }
  _S.CV.xr/=chart_xstep_denom;
  if(_S.CV.xr>=CHART_WIDTH) _S.CV.xr=CHART_WIDTH-1; 
  drawVertDashLine(_S.CV.xr, BLUE);
  if(_S.CV.xr>12) { // draw midnight lines  
    {
    DateTime now = RTC.now();
    i=now.dayOfWeek();
    y0 = now.hour()*60+now.minute(); // midday
    }
    while(1) {  
      x0=_S.CV.xr-y0/chart_xstep_denom;
      if(x0<0) break;
      drawVertDashLine(x0, YELLOW);
      lcd_defaults();  
      line_setpos(x0, 224);
      line_printn(DateTime::dayOfWeekStr(i));
      y0+=1440; // mins in 24h
      if(i) i--; else i=6;
    }
  }   
      
  Tft.setThick(5);  
  for(i=1; i<=TH_SID_SZ; i++) { // i for sid
    Tft.setColor(cc[i-1]);
    x0=0; y0=-1;
    mHist.iterBegin(i); 
    if(mHist.movePrev()) do {
      _S.CV.x1=_S.CV.xr-mHist.getPrevMinsBefore()/chart_xstep_denom;
      if(!TH_ISGAP_P(mHist.getPrev())) {
        _S.CV.y1=(int32_t)(_S2.CMM.maxt-mHist.getPrev()->getVal(GETCHRT()))*CHART_HEIGHT/(_S2.CMM.maxt-_S2.CMM.mint)+CHART_TOP;
        if(x0>0 && y0>=0) Tft.drawLineThickLowRAM8Bit(_S.CV.x1,_S.CV.y1,x0,y0);  
        y0=_S.CV.y1;
      }
      x0=_S.CV.x1; 
    } while(mHist.movePrev() && x0>0);
  } //for sid    
 
  } // --scope 1  
}

void chartHist60() 
{    
  const uint16_t DUR_24=24;
  const uint16_t DUR_MIN=60;
  const int16_t xstep = CHART_WIDTH/DUR_24;

  { // histogramm scope
  prepChart(pageidx+1, TH_HIST_VAL_T, (uint16_t)DUR_24*60+60);  
  if(_S2.CMM.maxt==_S2.CMM.mint) return;
  
  Tft.setBgColor(cc[pageidx]);
  
  { // inner scope
  uint8_t cnt=0;  
  uint8_t islot=0;
  uint8_t is;
  _S.HV.acc=0; // unionize with buf
  
  mHist.iterBegin(pageidx+1);
  do {
    if(mHist.movePrev()) is=mHist.getPrevMinsBefore()/DUR_MIN;
    else break;
    if(is>DUR_24) is=DUR_24;
    if(is!=islot) {  
      // draw prev;
      if(cnt) {
        _S.HV.y0=(int32_t)_S2.CMM.maxt*CHART_HEIGHT/(_S2.CMM.maxt-_S2.CMM.mint); // from top
        _S.HV.acc/=cnt;        
        if(_S.HV.acc<0) {
          if(_S2.CMM.maxt<0) { _S.HV.y0=0; _S.HV.h=_S2.CMM.maxt-_S.HV.acc; } else { _S.HV.h=-_S.HV.acc; } 
        }
        else {
          if(_S2.CMM.mint>0) { _S.HV.y0=CHART_HEIGHT; _S.HV.h=_S.HV.acc-_S2.CMM.mint;} else {_S.HV.h=_S.HV.acc; }
        }
        _S.HV.h=(int32_t)_S.HV.h*CHART_HEIGHT/(_S2.CMM.maxt-_S2.CMM.mint);
        if(_S.HV.acc>0) _S.HV.y0-=_S.HV.h;     
        _S.HV.acc=CHART_WIDTH-xstep*is+1;
        if(_S.HV.acc>=0)
          Tft.fillRectangle(_S.HV.acc, CHART_TOP+_S.HV.y0, xstep*(is-islot)-2, _S.HV.h);
      }      
      islot=is;
      _S.HV.acc=0;
      cnt=0;     
    }    
   if(!TH_ISGAP_P(mHist.getPrev())) { 
      _S.HV.acc+=mHist.getPrev()->getVal(TH_HIST_VAL_T);
      cnt++;          
   }
  } while(mHist.isNotOver() && islot<DUR_24);   
  } // inner scope
  } // hist scope  
  { // time labels scope
  lcd_defaults();
  DateTime now = RTC.now();  
  //printTime(now, true, CHART_WIDTH, 224, 2);   
  printTime2(now, CHART_WIDTH, 224, 2);   
  uint16_t mid = now.hour()+1; // unionize with buf
  drawVertDashLine(CHART_WIDTH-xstep*mid, YELLOW); // draw midnight line
  mid=mid>12 ? mid-12 : mid+12;
  drawVertDashLine(CHART_WIDTH-xstep*mid, RED); // draw noon line
 }
}


int8_t startIter(uint8_t sid) {  
  mHist.iterBegin(sid);  
  if(!mHist.movePrev()) {
    dispStat("NO DATA");
    return 0;
  }
  else return 1;
}


void prepChart(uint8_t  s, uint8_t type, uint16_t m) {
  _S2.CMM.maxt=_S2.CMM.mint=0;
  if(!startIter(s)) return;
  
  line_init(); 
  _S2.CMM.maxt=-9999; _S2.CMM.mint=9999; // VERY BAD!!!!
  
  { // ++scope 1
  uint8_t sid0, sid1;
  if(s!=0xFF) { sid0=sid1=s; line_printn(itoas(s)); line_printn(": "); }
  else {sid0=1; sid1=TH_SID_SZ; }
  
  do {
    mHist.iterBegin(sid0);
    if(mHist.movePrev()) {  
      do {
        if(!TH_ISGAP_P(mHist.getPrev())) {
          int16_t t = mHist.getPrev()->getVal(type);
          if(t>_S2.CMM.maxt) _S2.CMM.maxt=t;
          if(t<_S2.CMM.mint) _S2.CMM.mint=t;
        }
      } while(mHist.movePrev() && mHist.getPrevMinsBefore()<m);
    }
  } while(++sid0<=sid1);
  } // --scope 1
  
  line_printn(printVal(type, _S2.CMM.mint)); line_printn("..."); line_printn(printVal(type, _S2.CMM.maxt)); 
  
  if(_S2.CMM.mint%50) {
    if(_S2.CMM.mint>0) _S2.CMM.mint=(_S2.CMM.mint/50)*50;
     else _S2.CMM.mint=(_S2.CMM.mint/50-1)*50;
  }  
  if(_S2.CMM.maxt%50) {
     if(_S2.CMM.maxt>0) _S2.CMM.maxt=(_S2.CMM.maxt/50+1)*50;
     else _S2.CMM.maxt=(_S2.CMM.maxt/50)*50;
  }  
  if(_S2.CMM.maxt==_S2.CMM.mint) {_S2.CMM.mint-=50; _S2.CMM.maxt+=50;}  
  
  Tft.setColor(WHITE); 
  Tft.drawRectangle(0, CHART_TOP, CHART_WIDTH, CHART_HEIGHT);
  { // ++scope 2
  s=(_S2.CMM.maxt-_S2.CMM.mint)>100 ? 50 : 10;  
  for(int16_t ig=_S2.CMM.mint; ig<=_S2.CMM.maxt; ig+=s) { // degree lines
     m=CHART_TOP+(int32_t)(_S2.CMM.maxt-ig)*CHART_HEIGHT/(_S2.CMM.maxt-_S2.CMM.mint);
     if(ig>_S2.CMM.mint && ig<_S2.CMM.maxt) {
       Tft.setColor(ig==0? BLUE : GREEN);
       Tft.drawHorizontalLine(0, m, CHART_WIDTH);
     }
     lcd_defaults();
     line_setpos(CHART_WIDTH+1, m-FONT_Y); line_printn(printVal(type,ig));
  }
 
  } // --scope 2
}


void drawVertDashLine(int x, uint16_t color) {
  Tft.setColor(color);
  Tft.drawVerticalLine(x, CHART_TOP, CHART_HEIGHT);
}

void lcd_defaults() {
  Tft.setBgColor(BLACK);
  Tft.setFgColor(GREEN);
  Tft.setSize(WS_CHAR_DEF_SIZE);
  Tft.setOpaq(LCD_OPAQ);
}
 
inline uint16_t line_getpos() { return _lp_vpos; }
inline uint16_t line_getposx() { return _lp_hpos; }

inline void line_init() {
  _lp_vpos=_lp_hpos=0;
}

void line_print(const char* pbuf) {
  line_printn(pbuf);
  _lp_hpos=0;
  _lp_vpos+=FONT_Y*Tft.getSize();
}

inline void line_setpos(uint16_t x, uint16_t y) {
  _lp_hpos=x; _lp_vpos=y;
}

inline void line_setcharpos(uint8_t x) {
  _lp_hpos=(uint16_t)x*Tft.getSize()*FONT_SPACE;
}

void line_printn(const char* pbuf) {
  _lp_hpos = Tft.drawString(pbuf ,_lp_hpos, _lp_vpos);
}

/****************** HIST ****************/


char *itoas(uint8_t i) {
  return itoa(i, _S.buf, 10);
}

char *itoas2(uint8_t i) {
  if(i<10) { *_S.buf='0'; itoa(i, _S.buf+1, 10); }
  else itoa(i, _S.buf, 10);
  return _S.buf;
}

/****************** RADIO ****************/

uint8_t radioSetup() {
  uint8_t err=0;
  if (!nrf24.init()) err=1;
  else if (!nrf24.setChannel(WS_CHAN)) err=2;
  else if (!nrf24.setThisAddress((uint8_t*)addr, strlen(addr))) err=3;
  else if (!nrf24.setPayloadSize(sizeof(wt_msg))) err=4;
  //else if (!nrf24.setRF(NRF24::NRF24DataRate250kbps, NRF24::NRF24TransmitPower0dBm)) err=5;          
  else if (!nrf24.setRF(NRF24::NRF24DataRate250kbps, NRF24::NRF24TransmitPowerm6dBm)) err=5;          
  else { 
    nrf24.spiWriteRegister(NRF24_REG_00_CONFIG, nrf24.spiReadRegister(NRF24_REG_00_CONFIG)|NRF24_MASK_TX_DS|NRF24_MASK_MAX_RT); // only DR interrupt
    err=0;
  }  
  return err;
}

uint8_t radioRead() {
  wt_msg msg; // eventually, will move it here
  uint8_t err=0;
  uint8_t len=sizeof(msg); 
  if(!nrf24.available()) { 
   err=6; alarms |= WS_ALR_WFAIL; 
   if(flags & WS_FLAG_ONCEFAIL) { // double failure, reset radio
     flags &= ~WS_FLAG_ONCEFAIL;
     radioSetup();
     dispStat("RST RAD");     
   }
   else flags |= WS_FLAG_ONCEFAIL;
  } else {
    flags &= ~WS_FLAG_ONCEFAIL;
    if(!nrf24.recv((uint8_t*)&msg, &len)) { err=7; alarms |= WS_ALR_WFAIL;}
    else if(sizeof(msg) != len) { err=8; alarms |= WS_ALR_BAD_LENGTH;}
    else if(WS_MSG_TEMP != msg.msgt) { err=9; alarms |= WS_ALR_BADMSG;}
    else if(DS18_MEAS_FAIL == msg.msgt) {err=10;  alarms |= WS_ALR_BAD_TEMP;} // at the moment 
    else {
      msgcnt++;
      mHist.addAcc(msg.temp, msg.vcc, msg.sid, 0);
      err=0; alarms = 0;
    } 
  }
  return err;
}

void radioIRQ() {
  flags |= WS_FLAG_RFREAD;
}

