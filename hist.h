
// effective storage is TH_HIST_SZ-1
//#define TH_HIST_SZ  24
#define TH_HIST_SZ  26
#define TH_SID_SZ  2
//#define TH_ACC_TIME  14 //mins
#define TH_HIST_DV_T  5
#define TH_HIST_DV_V  2
#define TH_HIST_VAL_T 0
#define TH_HIST_VAL_V 1

#define TH_NODATA 0x0FFF
#define TH_SEMPTY 0xFFF
#define TH_ROLLUP_THR 244

#define TH_SETEMPTY(I) (hist[I].mins=TH_SEMPTY)
#define TH_ISEMPTY(I)  (hist[I].mins==TH_SEMPTY)

// uint16_t max: 65535 mins = 1092 hours = 45 days

class TempHistory {
public:
  class wt_msg_hist { // 4 bytes
    public:
    int16_t getVal(uint8_t type) { return type==TH_HIST_VAL_T ? temp*TH_HIST_DV_T : vcc*TH_HIST_DV_V; }
    uint16_t mins : 12; // mins since previous pu (0..254*16). 0xFFF=empty slot
    uint8_t sid : 4;  // src 0..15
    int8_t temp;    
    uint8_t vcc;    
  };
  struct wt_msg_acc {
    uint8_t cnt;
    uint8_t mins; // mins since previous put
    int16_t temp;
    uint16_t vcc;
  };

    TempHistory();
    void init();
    boolean addAcc(int16_t temp, int16_t vcc, uint8_t sid);
    uint8_t getSz();
    wt_msg_hist *getData(uint8_t sid, uint8_t pos);
    void iterBegin();
    boolean movePrev(); 
    wt_msg_hist *getPrev() { return hist+iter_ptr; }
    inline boolean isHead() {return !iter_ptr;} 
    inline boolean isNotOver() {return iter_ptr<TH_HIST_SZ && !TH_ISEMPTY(iter_ptr);} 
    uint16_t     getPrevMinsBefore() { return iter_mbefore; }
    uint8_t  getHeadDelay(uint8_t sid);
    uint8_t check(); // check if corrupted 
    static uint8_t interval_m(uint8_t prev);
protected:
    uint16_t compress(uint8_t level);
    wt_msg_hist hist[TH_HIST_SZ];
    uint8_t acc_prev_time_m[TH_SID_SZ];
    uint8_t iter_ptr;
    uint16_t iter_mbefore; 
};
