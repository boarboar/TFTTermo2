
// effective storage is TH_HIST_SZ-1
//#define TH_HIST_SZ  35
//#define TH_HIST_SZ  44
#define TH_HIST_SZ  46
#define TH_SID_SZ  2
#define TH_HIST_DV_T  5
//#define TH_HIST_DV_V  2
//#define TH_HIST_DV_V  32
//#define TH_HIST_BASE_V  224 //2.24v
#define TH_HIST_DV_V    17  // ca (512-base)/16
#define TH_HIST_BASE_V  240 //2.40v
#define TH_HIST_VAL_T 0
#define TH_HIST_VAL_V 1

#define TH_NODATA 0x0FFF
//#define TH_SEMPTY 0xFFF
#define TH_SEMPTY 0x3FF
#define TH_ROLLUP_THR 375 // must be less than TH_SEMPTY
#define TH_VALID_THR  120 // 2hrs
#define TH_GAP        60 // 1hrs, for test only

#define TH_SETEMPTY(I) (hist[(I)].mins=TH_SEMPTY)
#define TH_ISEMPTY(I)  (hist[(I)].mins==TH_SEMPTY)

#define TH_ISGAP(I)  (hist[(I)].mins==0)
#define TH_ISGAP_P(P)  ((P)->mins==0)

// uint16_t max: 65535 mins = 1092 hours = 45 days

class TempHistory {
public:
  class  __attribute__((__packed__)) wt_msg_hist { // 4 bytes
    public:
    int16_t getVal(uint8_t type) { return type==TH_HIST_VAL_T ? temp*TH_HIST_DV_T : vcc*TH_HIST_DV_V+TH_HIST_BASE_V; }
    /*
    uint16_t mins : 12; // mins since previous put (0..254*16). 0xFFF=empty slot
    uint8_t sid : 4;  // src 0..15
    int8_t temp;    
    uint8_t vcc;    
    */
    
    uint16_t mins : 10; // mins since previous put (0..254*4). 0x3FF=empty slot/ Max value = 0x3FE=1022
    uint8_t sid : 2;  // src 0..3. 0 - not valid?
    uint8_t vcc : 4;    
    int8_t  temp;    
    
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
