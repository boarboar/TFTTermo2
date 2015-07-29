
// effective storage is TH_HIST_SZ-1
//#define TH_HIST_SZ  33
#define TH_HIST_SZ  35
#define TH_SID_SZ  2
#define TH_HIST_DV_T  5
#define TH_HIST_DV_V  2
//#define TH_HIST_DV_V  8
#define TH_HIST_BASE_V  200 //2.00v
#define TH_HIST_VAL_T 0
#define TH_HIST_VAL_V 1

#define TH_NODATA 0x0FFF
#define TH_SEMPTY 0xFFF
//#define TH_SEMPTY 0xFF
#define TH_ROLLUP_THR 375

#define TH_SETEMPTY(I) (hist[I].mins=TH_SEMPTY)
#define TH_ISEMPTY(I)  (hist[I].mins==TH_SEMPTY)

// uint16_t max: 65535 mins = 1092 hours = 45 days

class TempHistory {
public:
  class  __attribute__((__packed__)) wt_msg_hist { // 4 bytes
    public:
    int16_t getVal(uint8_t type) { return type==TH_HIST_VAL_T ? temp*TH_HIST_DV_T : vcc*TH_HIST_DV_V+TH_HIST_BASE_V; }
    
    uint16_t mins : 12; // mins since previous put (0..254*16). 0xFFF=empty slot
    uint8_t sid : 4;  // src 0..15
    int8_t temp;    
    uint8_t vcc;    
    
    /*
    uint8_t mins; // mins since previous put (0..254). 0xFF=empty slot
    int8_t  temp;    
    uint8_t sid : 2;  // src 0..3
    uint8_t vcc : 6;    
    */
    };
    TempHistory();
    void init();
    boolean addAcc(int16_t temp, int16_t vcc, uint8_t sid);
    uint8_t getSz();
    wt_msg_hist *getData(uint8_t sid, uint8_t pos);
    void iterBegin(uint8_t sid);
    boolean movePrev(); 
    wt_msg_hist *getPrev() { return hist+iter_ptr; }
    inline boolean isHead() {return !iter_ptr;} 
    inline boolean isNotOver() {return iter_ptr<TH_HIST_SZ && !TH_ISEMPTY(iter_ptr);} 
    uint16_t     getPrevMinsBefore() { return iter_mbefore; }
    uint8_t  getHeadDelay(uint8_t sid);
    inline uint8_t  getLatestSid() {return hist[0].sid; }
    uint8_t check(); // check if corrupted 
    static uint8_t interval_m(uint8_t prev);
protected:
    uint16_t compress(uint8_t level);
    uint16_t iter_mbefore;     
    wt_msg_hist hist[TH_HIST_SZ];    
    uint8_t iter_ptr;
    uint8_t iter_sid;
    uint8_t acc_prev_time_m[TH_SID_SZ];
};
