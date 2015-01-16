
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
    //int32_t temp;
    //int32_t vcc;
    int16_t temp;
    uint16_t vcc;
  };

    TempHistory();
    void init();
    boolean addAcc(int16_t temp, int16_t vcc);
    //void add(uint8_t sid, uint8_t mins, int16_t temp, int16_t vcc);
    int16_t getDiff(int16_t val, uint8_t sid);
//    uint8_t getSz() { return head_ptr + (TH_HIST_SZ-1-tail_ptr); } // bullshit????
     uint8_t getSz();
    //unsigned long getLastAccTime() { return acc_prev_time; }
    wt_msg_hist *getData() { return hist; }
//    uint8_t getPrev(uint8_t pos) { return pos==0?TH_HIST_SZ-1 : pos-1; }
//    uint8_t getNext(uint8_t pos) { return pos==TH_HIST_SZ-1 ? 0 : pos+1; }
    void iterBegin();
    boolean movePrev(); 
    wt_msg_hist *getPrev() { return hist+iter_ptr; }
    uint16_t     getPrevMinsBefore() { return iter_mbefore; }
        
//    uint8_t _getHeadPtr() { return head_ptr; }
//    uint8_t _getTailPtr() { return tail_ptr; }
//    uint8_t _getSince1HAcc() { return since_1h_acc; }
//    uint8_t _getLast1HAccPtr() { return last_1h_acc_ptr; }
 //   uint8_t _getSince3HAcc() { return since_3h_acc; }
 //   uint8_t _getLast3HAccPtr() { return last_3h_acc_ptr; }
    
    static uint8_t interval_m(uint8_t prev);
protected:
    uint16_t compress(uint8_t level);

    wt_msg_hist hist[TH_HIST_SZ];
    
 //   uint8_t head_ptr; // NEXT record to fill
 //   uint8_t tail_ptr; // record before oldest one 
    
  //  uint16_t since_1h_acc; // mins passed after last 1-hour compression // uint16 ???????????
  //  uint16_t since_3h_acc; // mins passed after last 3-hour compression // uint16 ???????????
  //  uint8_t since_1h_acc; // mins passed after last 1-hour compression
  //  uint8_t last_1h_acc_ptr; // ptr to the last non-accumulated 1-hour item
  //  uint8_t last_3h_acc_ptr; // ptr to the last non-accumulated 3-hour item    
    wt_msg_acc acc;
    uint8_t acc_prev_time_m;
    uint8_t iter_ptr;
    uint16_t iter_mbefore; 
};
