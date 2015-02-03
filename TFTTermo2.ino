#include <SPI.h>
#include "NRF24.h"
#include "RTClibSS.h"
#include "TFT_ILI9341.h"
#include "hist.h"


// git push -u origin master //

// 5523 PINOUT

// SCK  P1_5
// MOSI P1_7
// MISO P1_6

#define TFT_CS_PIN P2_0
#define TFT_DC_PIN P2_5 // this can be reused 
#define TFT_BL_PIN P1_4 // can use RED for it...  (or use RED for IRQ ?)
  
#define DS1302_SCLK_PIN   P2_4    
#define DS1302_IO_PIN     P2_5    
#define DS1302_CE_PIN     P2_2   

#define NRF_CE_PIN P2_3
#define NRF_SS_CSN_PIN P2_1
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

#define WS_ALR_WFAIL_IDX  0
#define WS_ALR_BADMSG_IDX 1
#define WS_ALR_BAD_TEMP_IDX 2
#define WS_ALR_TO_IDX     3
#define WS_ALR_LOWVCC_IDX 4
#define WS_ALR_LAST_IDX 4

#define WS_SENS_TIMEOUT_M 10 // minutes

#define DS18_MEAS_FAIL	9999  // Temporarily!

// flags - only high bits, low bits reserved for value type
#define WS_FLAG_RFREAD    0x10
#define WS_FLAG_ONCEFAIL  0x20

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

#define WS_UI_CYCLE 50 
#define WS_DISP_CNT    10  // in UI_CYCLEs (=0.5s)
#define WS_INACT_CNT   40  // in DISP_CYCLEs (= 20s)

#define WS_CHART_NLEV 4

#define WS_BUT_NONE  0x00
#define WS_BUT_PRES  0x01

#define WS_BUT_MAX   13 
#define WS_BUT_CLICK 14 
#define WS_BUT_LONG 15

#define CHART_WIDTH 240
#define CHART_HEIGHT 204
#define CHART_TOP 19

#define GETHBHI(B) ((B)>>4)
#define GETHBLO(B) ((B)&0x0f)
#define SETHBHI(B, H) (B=(B&0x0f)+((H)<<4))
#define SETHBLO(B, L) (B=(B&0xf0)+((L)&0x0f))

//#define SETERR(E)   SETHBLO(flags, (E))
//#define GETERR()   GETHBLO(flags)

#define SETCHRT(T)   SETHBLO(flags, (T))
#define GETCHRT()   GETHBLO(flags)

const char *addr="wserv";

const char *lnames[] = {"main", "stat", "hist", "chart", "chr60", "vcc", "set"};

struct wt_msg {
  uint8_t msgt;
  uint8_t sid;
  int16_t temp;
  int16_t vcc;
}; 

NRF24 nrf24(NRF_CE_PIN, NRF_SS_CSN_PIN);
RTC_DS1302 RTC(DS1302_CE_PIN, DS1302_IO_PIN, DS1302_SCLK_PIN);

TempHistory mHist;
wt_msg msg = {0xFF, 0xFF, 0xFFFF, 0xFFFF};

uint8_t err=0; 
volatile uint8_t flags=0;

uint8_t alarms=0;
uint16_t msgcnt=0;


// ************************ HIST
int16_t last_tmp=-999, prev_tmp=TH_NODATA;
int16_t last_vcc=0, prev_vcc=-1;
//uint32_t last_millis_temp=0; // to sec, int?
uint16_t last_temp_cnt=0;

// ************************ UI

uint32_t mui;
uint32_t rts=0; 

uint8_t inact_cnt=0; // compress ?
uint8_t disp_cnt=0;  // compress ?

uint8_t editmode=0; // 0..4 compress ?

int8_t btcnt=0; // lowhalf-BUT1 cnt. hihalf-BUT2 cnt.

uint8_t uilev=0;   // compress ?
uint8_t pageidx=0; // compress ? combine with editmode?
//uint8_t chrt=TH_HIST_VAL_T; // compress  (add to flag as LO half)

uint16_t _lp_vpos=0;
int16_t _lp_hpos=0;

int16_t mint, maxt; // this is for charting

// change buf 6, buf1 4
/*
char buf[8];
char buf1[6];
*/
char buf[6];
char buf1[4];

// UNIONIZE!!! msg, buf, local time etc...

void setup()
{
  delay(100);
   // disable XTAL
  P2SEL &= ~BIT6; 
  P2SEL &= ~BIT7;
 
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
    
  Tft.TFTinit(TFT_CS_PIN, TFT_DC_PIN);
  Tft.setOrientation(LCD_LANDSCAPE);
  lcd_defaults();
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
  
  dispStat("INIT CLK");   
  RTC.begin();
  
  dispStat("INIT NRF");
  radioSetup();
  pinMode(NRF_IRQ_PIN, INPUT);  
  attachInterrupt(NRF_IRQ_PIN, radioIRQ, FALLING); 
  
  if(!err) dispStat("INIT OK.");  
  else dispErr();
 
  mHist.init(); 
  mui=millis();
  
  rts = RTC.now().unixtime();
  updateScreen(true);
}

void loop()
{
  if(flags&WS_FLAG_RFREAD) {
   flags &= ~WS_FLAG_RFREAD;
   radioRead();  
   if(err) dispErr();
   else {
     updateScreen(false);
     dispStat("READ OK.");
   }
  }

  unsigned long ms=millis(); 
  if(ms-mui>WS_UI_CYCLE || ms<mui) { // UI cycle
   mui=ms;
   last_temp_cnt++; 
   if(uilev>0 && !editmode && !inact_cnt) {  // back to main screen after user inactivity timeout
     uilev=0;
     updateScreen(true);
   }
   uint8_t btcnt_t;
   btcnt_t=processClick(BUTTON_1, GETHBLO(btcnt));
   if(btcnt_t>WS_BUT_MAX) {
     if(btcnt_t==WS_BUT_CLICK) processShortClick(); 
     else processLongClick();
     btcnt_t=0;     
   }
   SETHBLO(btcnt, btcnt_t);
   
   btcnt_t=processClick(BUTTON_2, GETHBHI(btcnt));
   if(btcnt_t>WS_BUT_MAX) {
     if(btcnt_t==WS_BUT_CLICK) processShortRightClick(); 
     btcnt_t=0;     
   }
   SETHBHI(btcnt, btcnt_t);
   
    
   if(++disp_cnt>=WS_DISP_CNT) { // 0.5 sec screen update   
     disp_cnt=0;   
     if(inact_cnt) inact_cnt--;
       if(!(alarms&WS_ALR_TO) && (uint32_t)last_temp_cnt*WS_UI_CYCLE/60000>WS_SENS_TIMEOUT_M) { // Alarm condition on no-data timeout            
         alarms |= WS_ALR_TO;
         if(uilev==WS_UI_MAIN) updateScreen(true);       
     }
     updateScreenTime(false);       
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
  if(!editmode) { 
    uilev=(uilev+1)%WS_NUILEV;
    updateScreen(true);
  }
  else {
    if(uilev==WS_UI_SET) {
      dispStat("EDIT MOV");
      hiLightDigit(BLACK);
      if(++editmode>4) editmode=1;
      hiLightDigit(WHITE);
    }
  }
}

void processLongClick() {
  if(uilev==WS_UI_SET) {
    if(!editmode) {
      dispStat("EDIT  ON");
      editmode=1;
      hiLightDigit(WHITE);
    } else {
      dispStat("EDIT OFF");
      timeStore();
      hiLightDigit(BLACK);
      editmode=0;
      dispStat("TIME STR");
    }
  } else {
    dispStat("LONG CLK");
  }
}

void processShortRightClick() {
  lcd_defaults();
  line_init();

  if(uilev==WS_UI_CHART || uilev==WS_UI_CHART_VCC) {
    if(++pageidx>=WS_CHART_NLEV) pageidx=0;
    Tft.fillScreen();
    //chartHist(1, pageidx, chrt);
    chartHist(1, pageidx, GETCHRT());
    return;
  }
  if(uilev==WS_UI_HIST) {
    Tft.fillScreen();
    pageidx=printHist(1, pageidx);
    return;
  }
  if(uilev!=WS_UI_SET) return;
  if(!editmode) return;
  timeUp(editmode-1, WS_CHAR_TIME_SET_SZ);
  hiLightDigit(WHITE);
}       

void updateScreen(bool refresh) {
  lcd_defaults();
  line_init();

  if(refresh) {
    Tft.fillScreen();
    Tft.drawString(lnames[uilev], 240, 0);
  }
   
  switch(uilev) {
    case WS_UI_MAIN: {      
     if((prev_tmp!=last_tmp || refresh) && last_tmp!=TH_NODATA) { 
       prev_tmp=last_tmp;
       int16_t t;
       Tft.setColor(alarms&WS_ALR_TO ? RED : GREEN);
       Tft.setSize(WS_CHAR_TEMP_SZ);
       t=Tft.drawString(printTemp(last_tmp), 0, 80);
       Tft.drawString("\177c ", t, 80);
       Tft.setColor(YELLOW);
       Tft.setSize(4);
       t=mHist.getDiff(last_tmp, 1);
       Tft.drawChar(t==0? ' ': t>0 ? WS_CHAR_UP : WS_CHAR_DN, FONT_SPACE*WS_CHAR_TEMP_SZ*7, 80);    
       lcd_defaults();
     }
     if(prev_vcc!=last_vcc || refresh) {
       prev_vcc=last_vcc;
       line_setpos(200, 211); line_printn(printVcc(last_vcc)); line_printn("v");
     }
     
     if(refresh) updateScreenTime(true);   
     }      
     break;
    case WS_UI_HIST: 
      pageidx=printHist(1, 0);
      break;    
    case WS_UI_CHART: {
      pageidx=0;
      SETCHRT(TH_HIST_VAL_T);
      chartHist(1, pageidx, TH_HIST_VAL_T);
      }
      break; 
    case WS_UI_CHART60: 
      chartHist60(1);
      break;
    case WS_UI_CHART_VCC: {
      pageidx=0;
      SETCHRT(TH_HIST_VAL_V);
      chartHist(1, pageidx, TH_HIST_VAL_V);
      }
      break;     
    case WS_UI_STAT: 
      printStat();    
      break;
    case WS_UI_SET: 
      updateScreenTime(true);  
      break;
    default: break;
  }
  
}

void updateScreenTime(bool reset) {
  uint8_t sz=0;
  
  if(uilev==WS_UI_MAIN) {
    sz=WS_CHAR_TIME_SZ;
    dispTimeout((uint32_t)last_temp_cnt*WS_UI_CYCLE/1000, reset, 0, 160);    
  } else if(uilev==WS_UI_SET) {
      if(!editmode) sz=WS_CHAR_TIME_SET_SZ;   
  }
  if(sz) {
    DateTime now = RTC.now();
    printTime(now, reset, 0, 40, sz);   
  }  
}

// buf 5
char *printTemp(int16_t disptemp) {
  char s='+';
  if(disptemp<0) {
    s='-';
    disptemp=-disptemp;
  } else if(disptemp==0) s=' ';
  buf[0]=s; 
  itoa((uint8_t)(disptemp/10), buf+1, 10);
  strcat(buf, ".");
  return strcat(buf, itoas((uint8_t)(disptemp%10)));
} 

// buf 4
char *printVcc(int16_t vcc) {
  itoa((uint8_t)(vcc/100), buf, 10);
  strcat(buf, ".");
  if(vcc%100<10) strcat(buf, "0");
  return strcat(buf, itoas((uint8_t)(vcc%100)));
}

char *printVal(uint8_t type, int16_t val) {
  if(type==TH_HIST_VAL_T) return printTemp(val);
  return printVcc(val);
}
    
//static byte p_date[3]={-1,-1,-1};
static byte p_time[3]={-1,-1,-1};
static byte p_to[3]={-1,-1,-1};
static byte p_days=-1;

// buf 4
void dispTimeout(uint32_t ts, bool reset, int x, int y) {
  byte tmp[3];  
  if(reset) p_days=-1;
  tmp[2]=ts%60; tmp[1]=(ts/60)%60; tmp[0]=(ts/3600)%24;
  uint8_t days=ts/3600/24;  
  if(days>0 && days!=p_days) {
    line_printn("> "); line_printn(itoas(days)); line_printn(" days");    
    p_days=days;
  }
  else {
     disp_dig(reset, 3, tmp, p_to, x, y, WS_CHAR_DEF_SIZE/*, ':', 0*/);
  }     
}

void printTime(const DateTime& pDT, bool reset, int x, int y, int sz/*, bool blinkd, bool printdate*/){
  Tft.setSize(sz);
  Tft.setColor(YELLOW);
  byte tmp[3];  
  tmp[0]=pDT.hour(); tmp[1]=pDT.minute(); tmp[2]=pDT.second();
  disp_dig(reset, 2, tmp, p_time, x, y, sz/*, (!blinkd || p_time[2]%2) ? ':' : ' ', tmp[2]!=p_time[2]*/);
  p_time[2]=tmp[2]; // store sec
  /*
  if(printdate) {
    tmp[0]=pDT.day(); tmp[1]=pDT.month(); tmp[2]=pDT.year()-2000;
    disp_dig(reset, 3, tmp, p_date, x+6*sz*FONT_SPACE, y, sz, '/', false);
  }*/
  lcd_defaults();
}

void timeUp(uint8_t dig, int sz) {
  uint8_t ig=dig/2;
  uint8_t id=(dig+1)%2; 
  uint8_t val=p_time[ig];
  uint8_t maxv[3]={24, 60, 60};
  if(id) { val+=10; if(val>maxv[ig]) val=val%10; }  
  else { val=(val/10)*10+((val%10)+1)%10; if(val>maxv[ig]) val=(val/10)*10;} 
  byte tmp[3];
  memcpy(tmp, p_time, 3);
  tmp[ig]=val;
  
  Tft.setSize(sz);
  Tft.setColor(YELLOW);
  disp_dig(false, 2, tmp, p_time, 0, 40, sz/*, ':', true*/);
}

void timeStore() {
  DateTime set=RTC.now();
  set.setTime(p_time[0], p_time[1], 0);
  RTC.adjust(set); 
}

/* 
redraw: always redraw
ngrp: number of digit groups (hh:mm = 2, hh:mm:ss, yy:mm:dd = 3). Always 2 digits in a group
data: ptr to new data. byte[ngrp]
pdata: ptr to old data. byte[ngrp]
x, y, sz - trivial
delim: delimeter symbol. shown before every group except of first
drdrm: always redraw separator (that's for blinking feature); 
*/

void disp_dig(byte redraw, byte ngrp, byte *data, byte *p_data, int x, int y, uint8_t sz) {
  int posX=x;
  for(byte igrp=0; igrp<ngrp; igrp++) {  
    if(igrp) {
      if(redraw) 
        Tft.drawChar(':', posX, y);  
      posX+=FONT_SPACE*sz;
    }
    for(byte isym=0; isym<2; isym++) {
       byte div10=(isym==0?10:1);
       byte dig=(data[igrp]/div10)%10;
       if(redraw || dig!=(p_data[igrp]/div10)%10)
         Tft.drawChar('0'+dig,posX,y); 
       posX+=FONT_SPACE*sz;
    }
    p_data[igrp]=data[igrp];
  }    
}

// buf 4
void dispErr() {
  Tft.setColor(RED);
  line_setpos(0, 211); line_printn("ERR="); line_printn(itoas(err));
  Tft.setColor(GREEN);
}

void dispStat(const char *pbuf) {
  line_setpos(0, 211); line_printn(pbuf);
}

void hiLightDigit(uint16_t color) {
  uint8_t d=(editmode>0 && editmode<3)?editmode-1:editmode;
  Tft.setColor(color);
  Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*d, 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ);
}


/****************** REPORTS ****************/

void printStat() {
   //uint32_t rtdur = RTC.now().unixtime()-rts;
   DateTime now=RTC.now(); 
   line_printn("NOW: "); line_printn(itoas(now.day())); line_printn("/"); line_printn(itoas(now.month())); line_printn("/"); line_print(itoas(now.year()));
   line_printn("UPT: "); dispTimeout(millis()/1000, true, line_getposx(), line_getpos()); line_print("");
   //line_printn("RTT: "); dispTimeout(rtdur, true, line_getposx(), line_getpos()); line_print("");
   line_printn("RTT: "); dispTimeout(now.unixtime()-rts, true, line_getposx(), line_getpos()); line_print("");
   mHist.iterBegin();  
   while(mHist.movePrev());
   line_printn("DUR: "); dispTimeout((uint32_t)mHist.getPrevMinsBefore()*60, true, line_getposx(), line_getpos()); line_print("");      
   line_printn("CNT="); 
   line_printn(itoa(msgcnt, buf, 10));
   //line_printn(itoas(msgcnt)); 
   line_printn(" HSZ="); line_print(itoas(mHist.getSz()));
   line_printn("TMO="); line_print(itoa(last_temp_cnt, buf, 10));  
   for(uint8_t i=0; i<=WS_ALR_LAST_IDX; i++) {
     line_printn("A");line_printn(itoas(i));line_printn("=");
     if(alarms&(1<<i)) line_print("Y");
     else line_print("N");
   }  
}
       
       
uint8_t printHist(uint8_t sid, uint8_t idx) {
  if(!startIter()) return 0; 
  uint8_t i=0;
  while(i<idx && mHist.movePrev()) i++;
  if(i<idx) { // wraparound
    mHist.iterBegin();
    mHist.movePrev();
    i=0;
  }  
  do {
    TempHistory::wt_msg_hist *h = mHist.getPrev();
    line_printn(itoas(i)); 
    line_setcharpos(4);
    line_printn(printTemp(h->getVal(TH_HIST_VAL_T))); //line_printn(" ");
    line_setcharpos(11);
    line_printn(printVcc(h->getVal(TH_HIST_VAL_V))); //line_printn(" ");    
    line_setcharpos(16);
    line_print(itoas(h->mins)); 
    i++;    
    } while(mHist.movePrev() && line_getpos()<230);
  return i;  
}


void chartHist(uint8_t sid, uint8_t scale, uint8_t type) {    
  const uint8_t chart_xstep_denoms[WS_CHART_NLEV]={7, 21, 49, 217};
  uint8_t chart_xstep_denom = chart_xstep_denoms[scale];
  uint16_t mbefore;
  prepChart(type, (uint16_t)CHART_WIDTH*chart_xstep_denom+60);
  if(maxt==mint) return;
  
  mbefore=mHist.getPrevMinsBefore(); // minutes passed after the earliest measurement
 
  int16_t xr, x0;     
  xr=(int32_t)mbefore/chart_xstep_denom;
  if(xr>=CHART_WIDTH) xr=CHART_WIDTH-1;
  
  drawVertDashLine(xr, BLUE);

  if(xr>36) { // draw midnight lines  // do something with this block. Too many local vars!
    DateTime now = RTC.now();
    uint16_t mid = now.hour()*60+now.minute();
    while(mid<mbefore) {
      x0=xr-(int32_t)mid/chart_xstep_denom;
      if(x0>0) drawVertDashLine(x0, YELLOW);
      mid+=1440; // mins in 24h
    }
    //DateTime start=DateTime(now.unixtime()-(uint32_t)mbefore*60);
    //printTime(start, true, 0, 224, 2);  
    now.shiftMins(-mbefore);
    printTime(now, true, 0, 224, 2);  
  }
  {
  int16_t y0;
  x0=y0=0;
  mHist.iterBegin(); mHist.movePrev();
  Tft.setColor(CYAN);
  Tft.setThick(5);
  do {
    int16_t y1=(int32_t)(maxt-mHist.getPrev()->getVal(type))*CHART_HEIGHT/(maxt-mint);
    int16_t x1=xr-mHist.getPrevMinsBefore()/chart_xstep_denom;
    if(!mHist.isHead()) {  
     if(x1>=0 && x0>0) 
       Tft.drawLineThick(x1,CHART_TOP+y1,x0,CHART_TOP+y0);
    }    
    x0=x1; y0=y1;
  } while(mHist.movePrev() && x0>0);     
  }
}

void chartHist60(uint8_t sid) 
{    
  const int DUR_24=24;
  const int DUR_MIN=60;
  const int xstep = CHART_WIDTH/DUR_24;

  { // histogramm scope
  prepChart(TH_HIST_VAL_T, (uint16_t)DUR_24*60+60);  
  if(maxt==mint) return;
  
  int16_t y_z=(int32_t)maxt*CHART_HEIGHT/(maxt-mint); // from top
  
  int16_t acc=0;
  uint8_t cnt=0;  
  uint8_t islot=0;
//line_init();
  Tft.setBgColor(WHITE);  
  mHist.iterBegin();  
  do {
    //uint8_t is=mHist.movePrev() ? mHist.getPrevMinsBefore()/DUR_MIN : DUR_24;
    //uint8_t is=mHist.movePrev() ? mHist.getPrevMinsBefore()/DUR_MIN : islot+1; // wrong! always getprevmins...    
    uint8_t is;
    if(mHist.movePrev()) is=mHist.getPrevMinsBefore()/DUR_MIN;
    //else { is=mHist.getPrevMinsBefore()/DUR_MIN; if(is==islot) is=islot+1; }
    else break;
    if(is>DUR_24) is=DUR_24;
    if(is!=islot) {  
      // draw prev;
      if(cnt) {
        acc/=cnt;
        int16_t y0=y_z;        
        int16_t h;
        if(acc<0) {
          if(maxt<0) { y0=0; h=maxt-acc; } else { h=-acc; } 
        }
        else {
          if(mint>0) { y0=CHART_HEIGHT; h=acc-mint;} else {h=acc; }
        }
        h=(int32_t)h*CHART_HEIGHT/(maxt-mint);
        if(acc>0) y0-=h;     
        
        //Tft.fillRectangle(CHART_WIDTH-xstep*(islot+1)+1, CHART_TOP+y0, xstep-2, h);
        /*
        // 24 0 10 38
           line_printn(itoa(is, buf, 10));
           line_printn(" ");
           line_printn(itoa(islot, buf, 10));
           line_printn(" ");
           line_printn(itoa(xstep, buf, 10));
           line_printn(" ");
           line_printn(itoa(xstep*(is-islot)-2, buf, 10));
           line_print(" ;");
        */
        //Tft.fillRectangle(CHART_WIDTH-xstep*(is+1)+1, CHART_TOP+y0, xstep*(is-islot)-2, h); //!!!
        acc=CHART_WIDTH-xstep*is+1;
        if(acc>=0)
          Tft.fillRectangle(acc, CHART_TOP+y0, xstep*(is-islot)-2, h);

      }      
      islot=is;
      acc=0;
      cnt=0;     
    }    
    acc+=mHist.getPrev()->getVal(TH_HIST_VAL_T);
    cnt++;          
  //} while(islot<DUR_24); 
  } while(mHist.isNotOver() && islot<DUR_24); // is<24
  
  }
  
  { // time labels scope
  lcd_defaults();
  DateTime now = RTC.now();  
  printTime(now, true, CHART_WIDTH, 224, 2);   
  uint16_t mid = now.hour()+1;
  drawVertDashLine(CHART_WIDTH-xstep*mid, YELLOW); // draw midnight line
  mid=mid>12 ? mid-12 : mid+12;
  drawVertDashLine(CHART_WIDTH-xstep*mid, RED); // draw noon line
  }
  
}


int8_t startIter() {  
  mHist.iterBegin();  
  if(!mHist.movePrev()) {
    dispStat("NO DATA");
    return 0;
  }
  else return 1;
}


void prepChart(uint8_t type, uint16_t mbefore) {
  if(!startIter()) return;  
  maxt=mint=mHist.getPrev()->getVal(type);
  do {
    int16_t t = mHist.getPrev()->getVal(type);
    if(t>maxt) maxt=t;
    if(t<mint) mint=t;
  } while(mHist.movePrev() && mHist.getPrevMinsBefore()<mbefore);

  line_init();
  line_printn(printVal(type, mint)); line_printn("..."); line_printn(printVal(type, maxt)); 
  
  if(mint%50) {
    if(mint>0) mint=(mint/50)*50;
     else mint=(mint/50-1)*50;
  }  
  if(maxt%50) {
     if(maxt>0) maxt=(maxt/50+1)*50;
     else maxt=(maxt/50)*50;
  }  
  if(maxt==mint) {mint-=50; maxt+=50;}  
  
  Tft.setColor(WHITE); 
  Tft.drawRectangle(0, CHART_TOP, CHART_WIDTH, CHART_HEIGHT);
  int16_t y0;  
//  uint8_t y0;
  y0=(maxt-mint)>100 ? 50 : 10;  
  for(int16_t ig=mint; ig<=maxt; ig+=y0) { // degree lines
     int16_t yl=CHART_TOP+(int32_t)(maxt-ig)*CHART_HEIGHT/(maxt-mint);
     //if(ig>mint && ig<maxt) Tft.drawStraightDashedLine(LCD_HORIZONTAL, 0, yl, CHART_WIDTH, ig==0? BLUE : GREEN, BLACK, 0x0f);
     if(ig>mint && ig<maxt) {
       Tft.setColor(ig==0? BLUE : GREEN);
       Tft.drawHorizontalLine(0, yl, CHART_WIDTH);
     }
     lcd_defaults();
     line_setpos(CHART_WIDTH+1, yl-FONT_Y); line_printn(printVal(type,ig));
  }
}


void drawVertDashLine(int x, uint16_t color) {
  Tft.setColor(color);
  Tft.drawVerticalLine(x, CHART_TOP, CHART_HEIGHT);
  //Tft.drawStraightDashedLine(LCD_VERTICAL, x, CHART_TOP, CHART_HEIGHT, color,BLACK, 0x0f); // 
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

uint8_t addHistAcc(struct wt_msg *pmsg) {
  last_vcc=pmsg->vcc;
  if(DS18_MEAS_FAIL==pmsg->temp) {
    alarms |= WS_ALR_BAD_TEMP;
    return 1;
  }
  msgcnt++;
  last_tmp=pmsg->temp; 
  last_temp_cnt=0;
  mHist.addAcc(last_tmp, last_vcc);
  return 0;
}

char *itoas(uint8_t i) {
  return itoa(i, buf1, 10);
}


/****************** RADIO ****************/

void radioSetup() {
  if (!nrf24.init()) err=1;
  else if (!nrf24.setChannel(WS_CHAN)) err=2;
  else if (!nrf24.setThisAddress((uint8_t*)addr, strlen(addr))) err=3;
  else if (!nrf24.setPayloadSize(sizeof(wt_msg))) err=4;
  //else if (!nrf24.setRF(NRF24::NRF24DataRate2Mbps, NRF24::NRF24TransmitPower0dBm))
  else if (!nrf24.setRF(NRF24::NRF24DataRate250kbps, NRF24::NRF24TransmitPower0dBm)) err=5;          
  else { 
    nrf24.spiWriteRegister(NRF24_REG_00_CONFIG, nrf24.spiReadRegister(NRF24_REG_00_CONFIG)|NRF24_MASK_TX_DS|NRF24_MASK_MAX_RT); // only DR interrupt
    err=0;
  }  
}

void radioRead() {
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
    else if(WS_MSG_TEMP != msg.msgt) { err=8; alarms |= WS_ALR_BADMSG;}
    else if(0==addHistAcc(&msg)) {err=0; alarms = 0;} // at the moment 
  }
}

void radioIRQ() {
  //rf_read=1; 
  flags |= WS_FLAG_RFREAD;
}
