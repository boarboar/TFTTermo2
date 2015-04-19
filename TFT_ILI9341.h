/*
 2012 Copyright (c) Seeed Technology Inc.

 Authors: Albert.Miao & Loovee, 
 Visweswara R (with initializtion code from TFT vendor)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 ILI9341
 
*/
#ifndef TFTv2_h
#define TFTv2_h

#if defined(ARDUINO) && ARDUINO >= 100
#define SEEEDUINO
#include <Arduino.h>
#else
#include <WProgram.h>
#endif
//#include <avr/pgmspace.h>

#include <SPI.h>

//Basic Colors
#define RED		0xf800
#define GREEN	0x07e0
#define BLUE	0x001f
#define BLACK	0x0000
#define YELLOW	0xffe0
#define WHITE	0xffff

//Other Colors
#define CYAN		0x07ff	
#define BRIGHT_RED	0xf810	
#define GRAY1		0x8410  
#define GRAY2		0x4208 

//TFT_RGB - take a traditional 8 bit per channel RGB colour, and turn it into 5/6/5 bit colour for the TFT
#define TFT_RGB(red, green, blue) (((((uint16_t)red>>3) & 0x001F)<<11)|((((uint16_t)green>>2) & 0x003F) << 5) | (((uint16_t)blue>>3) & 0x001F))

//TFT resolution 240*320
#define MIN_X	0
#define MIN_Y	0
#define MAX_X	239
#define MAX_Y	319

//!!!!!!!!!!! this should be fixed if flipXY

//SPI DEFS:
//MOSI P1_7
//MISO P1_6
//SCLK P1_5
//CS pin P2.0
//DC pin P2.1
//BL pin P2.2

/*
#define TFT_CS_LOW  {P2OUT &=~ 0x01;}
#define TFT_CS_HIGH {P2OUT |=  0x01;}
#define TFT_DC_LOW  {P2OUT &=~ 0x02;}
#define TFT_DC_HIGH {P2OUT |=  0x02;}
#define TFT_BL_OFF  {P2OUT &=~ 0x04;}
#define TFT_BL_ON   {P2OUT |=  0x04;}
*/


#define LCD_FLIP_X	0x01
#define LCD_FLIP_Y	0x02
#define LCD_SWITCH_XY   0x04
#define LCD_LANDSCAPE (LCD_SWITCH_XY+LCD_FLIP_Y)
#define LCD_PORTRAIT 0
#define LCD_ORIENTATION (LCD_FLIP_X+LCD_FLIP_Y+LCD_SWITCH_XY)
#define LCD_OPAQ        0x08        
#define LCD_TRANSP      0x00
#define LCD_BG          0x10        
#define LCD_FG          0x00

#define LCD_HORIZONTAL 0
#define LCD_VERTICAL 1

#ifndef INT8U
#define INT8U uint8_t
#endif
#ifndef INT16U
#define INT16U uint16_t
#endif
#ifndef INT16
#define INT16 int16_t
#endif

#define FONT_SPACE 6
#define FONT_Y 8
#define FONT_SZ 5

extern INT8U simpleFont[][FONT_SZ];

class TFT
{
public:
        void TFTinit (INT8U cs=P2_0, INT8U dc=P2_1);
	void setOrientation(int value);
        void fillScreen(void);
	void fillScreen(INT16 XL,INT16 XR,INT16 YU,INT16 YD);
	void drawChar(INT8U ascii,INT16U poX, INT16U poY);
        INT16U drawString(const char *string,INT16U poX, INT16U poY);
	inline void fillRectangle(INT16 poX, INT16 poY, INT16 length, INT16U width) { setFillColor(LCD_BG); fillScreen(poX, poX+length, poY, poY+width); }	
        void drawLineThick(INT16 x0,INT16 y0,INT16 x1,INT16 y1);
        void drawLineThickLowRAM(INT16 x0,INT16 y0,INT16 x1,INT16 y1);
        inline void drawVerticalLine(INT16 poX, INT16 poY,INT16 length) { setFillColor(LCD_FG); fillScreen(poX,poX,poY,poY+length);}  
        inline void drawHorizontalLine(INT16 poX, INT16 poY,INT16 length) {setFillColor(LCD_FG); fillScreen(poX,poX+length,poY,poY);}   
        //void drawStraightDashedLine(INT8U dir, INT16 poX, INT16 poY, INT16U length);
        inline void drawRectangle(INT16 poX, INT16 poY, INT16U length,INT16U width) { \
          setFillColor(LCD_FG); \
          fillScreen(poX,poX+length,poY,poY); \
          fillScreen(poX,poX+length,poY+width,poY+width); \
          fillScreen(poX,poX,poY,poY+width); \
          fillScreen(poX+length,poX+length,poY,poY+width); \
        }	
        inline INT16U  getMinX() { return 0; }
        inline INT16U  getMinY() { return 0; }
        inline INT16U  getMaxX() { return _flags&LCD_SWITCH_XY ? MAX_Y : MAX_X; }
        inline INT16U  getMaxY() { return _flags&LCD_SWITCH_XY ? MAX_X : MAX_Y; }
        
        // Ctx
        inline void setColor(INT16U fgcolor) {_fgColorH = fgcolor>>8; _fgColorL=fgcolor&0xff;}
        inline void setFgColor(INT16U fgcolor) {_fgColorH = fgcolor>>8; _fgColorL=fgcolor&0xff;}	
        inline void setBgColor(INT16U bgcolor) {_bgColorH = bgcolor>>8; _bgColorL=bgcolor&0xff;}
        inline void setOpaq(INT8U opaq) {_flags&=~LCD_OPAQ; _flags|=LCD_OPAQ&opaq;} 
        inline void setFillColor(INT8U opaq) {_flags&=~LCD_BG; _flags|=LCD_BG&opaq;}
        inline void setSize(INT8U v) {_size_mask_thick=v;}
        inline void setMask(INT8U v) {_size_mask_thick=v;}
        inline void setThick(INT8U v) {_size_mask_thick=v;}
        inline INT8U getSize() { return _size_mask_thick; }
protected:
        void WriteCmdSeq(const INT8U *data);
//        void setWindow(INT16U StartCol, INT16U EndCol, INT16U StartPage,INT16U EndPage);
	void sendCMD(INT8U index);
	void WRITE_DATA(INT8U data);
	void sendData(INT16U data);

	INT8U _cs, _dc;
       //Ctx 
        INT8U _flags;	
        INT8U _size_mask_thick;
	//INT16U _fgColor, _bgColor;
        INT8U _fgColorL, _fgColorH, _bgColorL, _bgColorH;
        
};

//extern TFT Tft;

#endif

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/


