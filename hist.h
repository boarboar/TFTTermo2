
// effective storage is TH_HIST_SZ-1
//#define TH_HIST_SZ  12
#define TH_HIST_SZ  14
//#define TH_HIST_SZ  254 
#define TH_ACC_TIME  15 //mins
#define TH_HIST_DV_T  5
#define TH_HIST_DV_V  2
#define TH_HIST_VAL_T 0
#define TH_HIST_VAL_V 1

#define TH_NODATA 0x0FFF
#define TH_EMPTY 0xFF

// uint16_t max: 65535 mins = 1092 hours = 45 days

class TempHistory {
public:
  class wt_msg_hist {
    public:
    int16_t getVal(uint8_t type) { return type==TH_HIST_VAL_T ? temp*TH_HIST_DV_T : vcc*TH_HIST_DV_V; }
 //   uint8_t sid;  // src
    uint8_t mins; // mins since previous put
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
    boolean addAcc(int16_t temp, int16_t vcc);
    int16_t getDiff(int16_t val, uint8_t sid);
     uint8_t getSz();
    wt_msg_hist *getData() { return hist; }
    void iterBegin();
    boolean movePrev(); 
    wt_msg_hist *getPrev() { return hist+iter_ptr; }
    uint16_t     getPrevMinsBefore() { return iter_mbefore; }
    static uint8_t interval_m(uint8_t prev);
protected:
    uint16_t compress(uint8_t level);
    wt_msg_hist hist[TH_HIST_SZ];
    wt_msg_acc acc;
    uint8_t acc_prev_time_m;
    uint8_t iter_ptr;
    uint16_t iter_mbefore; 
};
