// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
friend class RTC_DS1302;  
public:
    DateTime() {;}
    //DateTime (uint32_t t);
    DateTime (uint8_t year, uint8_t month, uint8_t day,
                uint8_t hour =0, uint8_t min =0, uint8_t sec =0);
    void setTime(uint8_t hour =0, uint8_t min =0, uint8_t sec =0);    
    //void shiftMins(int16_t m);    
    //DateTime (const char* date, const char* time);
    
    
    //uint16_t year() const       { return 2000 + yOff; }
    uint8_t year() const       { return yOff; } // after 2000
    uint8_t month() const       { return m; }
    uint8_t day() const         { return d; }
    uint8_t hour() const        { return hh; }
    uint8_t minute() const      { return mm; }
    uint8_t second() const      { return ss; }    
    uint8_t dayOfWeek() const;
    const char *dayOfWeekStr() const;
    
    // 32-bit times as seconds since 1/1/1970
    uint32_t unixtime(void) const;
    
    static const char *dayOfWeekStr(uint8_t dw);
protected:
    // save as unixtime!!!!
    uint8_t yOff, m, d, hh, mm, ss;
};

class RTC_DS1302 {
public:
    //RTC_DS1302(uint8_t ce_pin, uint8_t io_pin, uint8_t sclk_pin);
    void begin(void);
    void adjust(const DateTime& dt);
    DateTime now();
    void writeProtect(bool enable);
    void halt(bool value);
private:
enum Register {
    kSecondReg       = 0,
    kMinuteReg       = 1,
    kHourReg         = 2,
    kDateReg         = 3,
    kMonthReg        = 4,
    kDayReg          = 5,
    kYearReg         = 6,
    kWriteProtectReg = 7
  };

/*
  uint8_t ce_pin_;
  uint8_t io_pin_;
  uint8_t sclk_pin_;
*/

  uint8_t readRegister(Register reg);
  uint8_t readRegisterBcdToDec(Register reg, uint8_t high_bit); 
  inline uint8_t readRegisterBcdToDec(Register reg) {return readRegisterBcdToDec(reg, 7);}
  void writeRegister(Register reg, uint8_t value);
  void writeRegisterDecToBcd(Register reg, uint8_t value, uint8_t high_bit);
  inline void writeRegisterDecToBcd(Register reg, uint8_t value) {writeRegisterDecToBcd(reg, value, 7);}

  void writeOut(uint8_t value);
  uint8_t readIn();

};
