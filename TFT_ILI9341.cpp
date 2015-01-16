/*
 ILI9341
*/


#include "TFT_ILI9341.h"
#include <SPI.h>

#define TFT_CS_LOW  {digitalWrite(_cs, LOW);}
#define TFT_CS_HIGH {digitalWrite(_cs, HIGH);}
#define TFT_DC_LOW  {digitalWrite(_dc, LOW);}
#define TFT_DC_HIGH {digitalWrite(_dc, HIGH);}

void TFT::sendCMD(INT8U index)
{
    TFT_DC_LOW;
    TFT_CS_LOW;
    SPI.transfer(index);
    TFT_CS_HIGH;
}

void TFT::WRITE_DATA(INT8U data)
{
    TFT_DC_HIGH;
    TFT_CS_LOW;
    SPI.transfer(data);
    TFT_CS_HIGH;
}

void TFT::sendData(INT16U data)
{
    TFT_DC_HIGH;
    TFT_CS_LOW;
    SPI.transfer(data>>8);
    SPI.transfer(data&0xff);
    TFT_CS_HIGH;
    
}

void TFT::WriteCmdSeq(const INT8U *data)
{
  while(*data) {
    INT8U data_len1=(*data++);
    sendCMD(*data++);
    while(--data_len1) WRITE_DATA(*data++);    
  }
}

void TFT::TFTinit (INT8U cs, INT8U dc)
{
    _cs=cs;
    _dc=dc; 
    _flags = 0;
	
    pinMode(_cs,OUTPUT);
    pinMode(_dc,OUTPUT);
	
    SPI.begin();
    SPI.setClockDivider(2);
    TFT_CS_HIGH;
    TFT_DC_HIGH;
    
    delay(500);
    sendCMD(0x01);
    delay(200);    

    const INT8U seq[]={
      4, 0xCF,    0x00,0x8B,0x30,
      5, 0xED,    0x67,0x03,0x12,0x81,
      4, 0xE8,    0x85,0x10,0x7A,
      6, 0xCB,    0x39,0x2C,0x00,0x34,0x02,
      2, 0xF7,    0x20,
      3, 0xEA,    0x00, 0x00,
      2, 0xC0,    0x1B, // Power control     VRH[5:0]              
      2, 0xC1,    0x10, // Power control    SAP[2:0];BT[3:0]            
      3, 0xC5,    0x3F, 0x3C,  // VCM control                  
      2, 0xC7,    0xB7,       // VCM control2                 
      2, 0x36,    0x08,        // Memory Access Control   portrait     
      2, 0x3A,    0x55,
      3, 0xB1,    0x00, 0x1B,
      3, 0xB6,    0x0A, 0xA2,    // Display Function Control     
      2, 0xF2,    0x00,          // 3Gamma Function Disable      
      2, 0x26,    0x01,          // Gamma curve selected      
      16, 0xE0,   0x0F,0x2A,0x28,0x08,0x0E,0x08,0x54,0xA9,0x43,0x0A,0x0F,0x00,0x00,0x00,0x00, // Set Gamma                    
      16, 0xE1,   0x00,0x15,0x17,0x07,0x11,0x06,0x2B,0x56,0x3C,0x05,0x10,0x0F,0x3F,0x3F,0x0F, // Set Gamma
      0      
    };
    
    WriteCmdSeq(seq);

    sendCMD(0x11);                                                      /* Exit Sleep                   */
    delay(120);
    sendCMD(0x29);                                                      /* Display on                   */
    fillScreen();
}

void TFT::setOrientation(int flags) 
{
  INT8U madctl = 0x08;
  if(flags & LCD_FLIP_X) madctl &= ~(1 << 6);
  if(flags & LCD_FLIP_Y) madctl |= 1 << 7;
  if(flags & LCD_SWITCH_XY) madctl |= 1 << 5;
  sendCMD(0x36);
  WRITE_DATA(madctl);
  _flags &= ~LCD_ORIENTATION;
  _flags |= flags&LCD_ORIENTATION;
}

/*
void TFT::setWindow(INT16U StartCol, INT16U EndCol, INT16U StartPage,INT16U EndPage)
{
    sendCMD(0x2A);                                                      
    sendData(StartCol); 
    sendData(EndCol);
    sendCMD(0x2B);                                                      
    sendData(StartPage);
    sendData(EndPage);
    sendCMD(0x2c);
}
*/

void TFT::fillScreen(void)
{
   setBgColor(BLACK);
   setFillColor(LCD_BG);
   fillScreen(0, getMaxX(), 0, getMaxY());
}

void TFT::fillScreen(INT16 XL, INT16 XR, INT16 YU, INT16 YD)
{
    uint32_t  XY;
/*
    if(XL > XR) { XL = XL^XR; XR = XL^XR; XL = XL^XR;}
    if(YU > YD) { YU = YU^YD; YD = YU^YD; YU = YU^YD;}
	
    XL = constrain(XL,0,getMaxX());
    XR = constrain(XR,0,getMaxX());
    YU = constrain(YU,0,getMaxY());
    YD = constrain(YD,0,getMaxY());
*/		
 //   setWindow(XL,XR,YU,YD);
   
    sendCMD(0x2A);                                                      
    sendData(XL); 
    sendData(XR);
    sendCMD(0x2B);                                                      
    sendData(YU);
    sendData(YD);
    sendCMD(0x2c);
 
    XY = (XR-XL+1);
    XY *=(YD-YU+1);

    TFT_DC_HIGH;
    TFT_CS_LOW;
    
    if(_flags&LCD_BG) 
    while(XY--) {
      SPI.transfer(_bgColorH);
      SPI.transfer(_bgColorL);
    }
    else
    while(XY--) {
      SPI.transfer(_fgColorH);
      SPI.transfer(_fgColorL);
    }
   
    TFT_CS_HIGH;
}

void TFT::drawChar( INT8U ascii, INT16 poX, INT16 poY)
{   
    if(_flags&LCD_OPAQ) { setFillColor(LCD_BG); fillScreen(poX, poX+FONT_SPACE*_size_mask_thick, poY, poY+FONT_Y*_size_mask_thick); }	
    setFillColor(LCD_FG);
    if((ascii<32)||(ascii>129)) ascii = '?';
    INT16U x=poX;
    for (INT8U i=0; i<FONT_SZ; i++, x+=_size_mask_thick ) {
        INT8U temp = simpleFont[ascii-0x20][i];
        INT16U y=poY;
        for(INT8U f=0;f<8;f++, y+=_size_mask_thick)
        {
            if((temp>>f)&0x01) // if bit is set in the font mask
            {
              fillScreen(x, x+_size_mask_thick, y, y+_size_mask_thick);
            }
        }
    }
    setFillColor(LCD_BG);
}

INT16U TFT::drawString(const char *string,INT16 poX, INT16 poY)
{
    while(*string)
    {
        drawChar(*string, poX, poY);
        string++;
        if(poX < getMaxX()) poX += FONT_SPACE*_size_mask_thick;   /* Move cursor right */
        else break;
    }
    return poX;
}

/*
void TFT::drawStraightDashedLine(INT8U dir, INT16 poX, INT16 poY, INT16U length)
{
    // need to rework!!!
    if(dir==LCD_HORIZONTAL) setWindow(poX,poX + length,poY,poY);
    else setWindow(poX,poX,poY,poY + length);
    while(length--) sendData((_size_mask_thick>>(length&0x07))&0x01 ? _fgColor : _bgColor);
}
*/

void TFT::drawLineThick(INT16 x0,INT16 y0,INT16 x1,INT16 y1)
{   
    int16_t dx, dy;
    int16_t sx, sy;
    
    if(x0<x1) {dx=x1-x0; sx=1;} else {dx=x0-x1; sx=-1; }
    if(y0<y1) {dy=y0-y1; sy=1;} else {dy=y1-y0; sy=-1; }
    
    int16_t err = dx+dy, e2;                                                /* error value e_xy             */
    INT8U th2=_size_mask_thick/2;
    for (;;){                                                           /* loop                         */
        e2 = 2*err;
        if (e2 >= dy) {                   /* e_xy+e_x > 0                 */
            drawVerticalLine(x0, y0-th2, _size_mask_thick);
            if (x0 == x1) break;
            err += dy; x0 += sx;
        }
        if (e2 <= dx) {                   /* e_xy+e_y < 0                 */
            drawHorizontalLine(x0-th2, y0, _size_mask_thick/*, color*/);
            if (y0 == y1) break;
            err += dx; y0 += sy;
        }
    }
}
        
/*        
void TFT::drawRectangle(INT16 poX, INT16 poY, INT16U length, INT16U width)
{
    drawHorizontalLine(poX, poY, length);
    drawHorizontalLine(poX, poY+width, length);
    drawVerticalLine(poX, poY, width);
    drawVerticalLine(poX + length, poY, width);

}
*/

TFT Tft=TFT();
/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
