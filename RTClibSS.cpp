
#include <Energia.h>// capital A so it is error prone on case-sensitive filesystems
#include "RTClibSS.h"

#define sclk_pin_   P2_4    
#define io_pin_     P2_5    
#define ce_pin_     P2_2   


//#define DS1307_ADDRESS 0x68
#define SECONDS_PER_DAY 86400L

#define SECONDS_FROM_1970_TO_2000 946684800

////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

const static uint8_t daysInMonth [] = { 31,28,31,30,31,30,31,31,30,31,30,31 }; //has to be const or compiler compaints
const static char* weekDays[7]={"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

// number of days since 2000/01/01, valid for 2001..2099

static uint16_t date2days(uint8_t y, uint8_t m, uint8_t d) {
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += daysInMonth[i - 1];
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

inline static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

const char *DateTime::dayOfWeekStr(uint8_t dw) { return weekDays[dw]; }

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
    yOff = year;
    m = month;
    d = day;
    hh = hour;
    mm = min;
    ss = sec;
}

void DateTime::setTime (uint8_t hour, uint8_t min, uint8_t sec) {
    hh = hour;
    mm = min;
    ss = sec;
}

void DateTime::setDate (uint8_t day, uint8_t month, uint8_t year) {
    d = day;
    m = month;
    yOff = year;
}

uint8_t DateTime::dayOfWeek() const {    
    uint16_t day = date2days(yOff, m, d);
    return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::unixtime(void) const {  
  uint16_t days = date2days(yOff, m, d);
  return time2long(days, hh, mm, ss)+SECONDS_FROM_1970_TO_2000;
  //return ((days * 24L + hh) * 60 + mm) * 60 + ss + SECONDS_FROM_1970_TO_2000;
}


const char *DateTime::dayOfWeekStr() const {
  return weekDays[dayOfWeek()];
}
    
////////////////////////////////////////////////////////////////////////////////
// RTC_DS1307 implementation

/*
RTC_DS1302::RTC_DS1302(const uint8_t ce_pin, const uint8_t io_pin,
               const uint8_t sclk_pin) {
  ce_pin_ = ce_pin;
  io_pin_ = io_pin;
  sclk_pin_ = sclk_pin;
}
*/

void RTC_DS1302::begin() {
  pinMode(ce_pin_, OUTPUT);
  pinMode(sclk_pin_, OUTPUT);
}

void RTC_DS1302::adjust(const DateTime& dt) {
  // Initialize a new chip by turning off write protection and clearing the
  // clock halt flag. These methods needn't always be called. See the DS1302
  // datasheet for details.
  writeProtect(false);
  halt(false);
  writeRegisterDecToBcd(kSecondReg, dt.ss, 6);
  writeRegisterDecToBcd(kMinuteReg, dt.mm, 6);
  writeRegister(kHourReg, 0);  // set 24-hour mode
  writeRegisterDecToBcd(kHourReg, dt.hh, 5);
  writeRegisterDecToBcd(kDateReg, dt.d, 5);
  writeRegisterDecToBcd(kMonthReg, dt.m, 4);
  //writeRegisterDecToBcd(kDayReg, dofweek, 2);
  writeRegisterDecToBcd(kYearReg, dt.yOff); // year-2000 
}

DateTime RTC_DS1302::now() {
  DateTime d;
  d.ss = readRegisterBcdToDec(kSecondReg, 6);
  d.mm = readRegisterBcdToDec(kMinuteReg);
  d.hh = readRegister(kHourReg);
  uint8_t adj;
  if (d.hh & 128)  // 12-hour mode
    adj = 12 * ((d.hh & 32) >> 5);
  else           // 24-hour mode
    adj = 10 * ((d.hh & (32 + 16)) >> 4);
  d.hh = (d.hh & 15) + adj;
  d.d = readRegisterBcdToDec(kDateReg, 5);
  d.m = readRegisterBcdToDec(kMonthReg, 4);
  d.yOff = readRegisterBcdToDec(kYearReg);
  
  if(d.yOff>32) d.yOff=15; // workaround ... remove it later
  
  return d;
}

void RTC_DS1302::writeProtect(const bool enable) {
  writeRegister(kWriteProtectReg, (enable << 7));
}

void RTC_DS1302::halt(const bool enable) {
  uint8_t sec = readRegister(kSecondReg);
  sec &= ~(1 << 7);
  sec |= (enable << 7);
  writeRegister(kSecondReg, sec);
}

uint8_t RTC_DS1302::readRegisterBcdToDec(const Register reg, const uint8_t high_bit) {
  const uint8_t mask = (1 << (high_bit + 1)) - 1;
  uint8_t val = readRegister(reg);
  val &= mask;
  val = (val & 15) + 10 * ((val & (15 << 4)) >> 4);
  return val;
}

void RTC_DS1302::writeRegisterDecToBcd(const Register reg, uint8_t value,
                              const uint8_t high_bit) {
  const uint8_t mask = (1 << (high_bit + 1)) - 1;
  uint8_t regv = readRegister(reg);

  // Convert value to bcd in place.
  uint8_t tvalue = value / 10;
  value = value % 10;
  value |= (tvalue << 4);

  // Replace high bits of value if needed.
  value &= mask;
  value |= (regv &= ~mask); //?????

  writeRegister(reg, value);
}

uint8_t RTC_DS1302::readRegister(const Register reg) {
  uint8_t cmd_byte = 129;  // 1000 0001
  uint8_t reg_value;
  cmd_byte |= (reg << 1);
  digitalWrite(sclk_pin_, LOW);
  digitalWrite(ce_pin_, HIGH);
  writeOut(cmd_byte);
  reg_value = readIn();
  digitalWrite(ce_pin_, LOW);
  return reg_value;
}

void RTC_DS1302::writeRegister(const Register reg, const uint8_t value) {
  uint8_t cmd_byte = (128 | (reg << 1));
  digitalWrite(sclk_pin_, LOW);
  digitalWrite(ce_pin_, HIGH);
  writeOut(cmd_byte);
  writeOut(value);
  digitalWrite(ce_pin_, LOW);
}

void RTC_DS1302::writeOut(const uint8_t value) {
  pinMode(io_pin_, OUTPUT);
  shiftOut(io_pin_, sclk_pin_, LSBFIRST, value);
}

uint8_t RTC_DS1302::readIn() {
  uint8_t input_value = 0;
  uint8_t bit = 0;
  pinMode(io_pin_, INPUT);
  // do not use shift-in, it eats more memory
  for (int i = 0; i < 8; ++i) {
    bit = digitalRead(io_pin_);
    input_value |= (bit << i);
    digitalWrite(sclk_pin_, HIGH);
    delayMicroseconds(1);
    digitalWrite(sclk_pin_, LOW);
  }
  pinMode(io_pin_, OUTPUT); // for compartibility with other consumers
  return input_value;
}

