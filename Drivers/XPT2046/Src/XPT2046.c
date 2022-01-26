/*
 * XPT2046.c
 *
 *  Created on: Jan 10, 2022
 *      Author: user
 */

/* Includes ------------------------------------------------------------------*/
#include "XPT2046.h"
/* Private types -------------------------------------------------------------*/
typedef struct Point {
	int x;
	int y;
} point_t;
typedef struct ReferencePoint {
	int xDisplay;
	int yDisplay;
	unsigned int xADC;
	unsigned int yADC;
} ref_point_t;
/* Private define ------------------------------------------------------------*/

#define POINT_USER -1  /*точка пользователя в колбеке */

#define CONTROL_STARTBIT			0b10000000
#define CONTROL_CHANNEL_Y			0b00010000
#define CONTROL_CHANNEL_X			0b01010000
#define CONTROL_CHANNEL_Z1			0b00110000
#define CONTROL_CHANNEL_Z2			0b01000000

#define CONTROL_MODE_8BIT		 	0b00001000
#define CONTROL_MODE_12BIT 			0b00000000
#define CONTROL_MODE_SINGLEENDED	0b00000100
#define CONTROL_MODE_DIFFERENTIAL	0b00000000

#define CONTROL_MODE_PD_BTW_MEASURE	0b00000000
#define CONTROL_MODE_REF_OFF_ADC_ON	0b00000001
#define CONTROL_MODE_REF_ON_ADC_OFF	0b00000010
#define CONTROL_MODE_REF_ON_ADC_ON	0b00000011

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/**
 * Система координат дисплея - стандартная декартовая в первой четверти
 * ^ Y+
 * |
 * |
 * |
 * "0,0"---> X+
 * Вместо 0,0 есс-но displayLeft, displayBottom
 */


// размеры пикселей дисплея
uint16_t _displayWidth;
uint16_t _displayHeight;
//Ну мало ли у кого координаты нуля в центре, задаём начальные точки
int16_t _displayBottom;
int16_t _displayLeft;

//чистые значения того, что было замеренно
uint16_t _xRaw;
uint16_t _yRaw;
uint16_t _z1Raw;
uint16_t _z2Raw;
float _deltaX;
float _deltaY;
float _deltaZ1;
float _deltaZ2;
float _xRawFiltered;
float _yRawFiltered;
float _z1RawFiltered;
float _z2RawFiltered;
float _xRawFilteredOld;
float _yRawFilteredOld;
float _z1RawFilteredOld;
float _z2RawFilteredOld;
int16_t _xDisplay;
int16_t _yDisplay;
uint8_t _isWaiting = 0;
int8_t _typeOfPoint;
uint32_t touchPressMoment;

ref_point_t _referencePoints[5]; //контрольные точки для калибровки

/* Private constants ---------------------------------------------------------*/
typedef enum {
	POINT_CENTER,
	POINT_TOPLEFT,
	POINT_TOPRIGHT,
	POINT_BOTTOMLEFT,
	POINT_BOTTOMRIGHT} reference_points;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

void XPT2046_init(uint16_t displayWidth, uint16_t displayHeight, int16_t displayLeft, int16_t displayBottom) {
	XPT2046_Wait(100);
	_displayBottom = displayBottom;
	_displayLeft = displayLeft;
	_displayWidth = displayWidth;
	_displayHeight = displayHeight;
	_typeOfPoint = -1;
	_isWaiting = 0;
	XPT2046_Select();
	XPT2046_SPI_send(CONTROL_STARTBIT);
	XPT2046_SPI_send(0);
	XPT2046_SPI_send(0x00);
	XPT2046_Deselect();
	XPT2046_Wait(1000);
}
/**
 * Сброс калибровки тача на условно заводские настройки - в зависимости от предзаводских испытаний - менять по усмотрению
 **/
void XPT2046_clearCalibrationData(){
	_referencePoints[POINT_CENTER].xADC = 2048;
	_referencePoints[POINT_CENTER].yADC = 2048;
	_referencePoints[POINT_CENTER].xDisplay = _displayLeft+_displayWidth/2;
	_referencePoints[POINT_CENTER].yDisplay = _displayBottom+_displayHeight/2;

	_referencePoints[POINT_TOPLEFT].xADC = 0;
	_referencePoints[POINT_TOPLEFT].yADC = 0;
	_referencePoints[POINT_TOPLEFT].xDisplay = _displayLeft;
	_referencePoints[POINT_TOPLEFT].yDisplay = _displayBottom+_displayHeight;

	_referencePoints[POINT_TOPRIGHT].xADC = 4095;
	_referencePoints[POINT_TOPRIGHT].yADC = 4095;
	_referencePoints[POINT_TOPRIGHT].xDisplay = _displayLeft+_displayWidth;
	_referencePoints[POINT_TOPRIGHT].yDisplay = _displayBottom+_displayHeight;

	_referencePoints[POINT_BOTTOMLEFT].xADC = 0;
	_referencePoints[POINT_BOTTOMLEFT].yADC = 0;
	_referencePoints[POINT_BOTTOMLEFT].xDisplay = _displayLeft;
	_referencePoints[POINT_BOTTOMLEFT].yDisplay = _displayBottom;

	_referencePoints[POINT_BOTTOMRIGHT].xADC = 0;
	_referencePoints[POINT_BOTTOMRIGHT].yADC = 4095;
	_referencePoints[POINT_BOTTOMRIGHT].xDisplay = _displayLeft+_displayWidth;
	_referencePoints[POINT_BOTTOMRIGHT].yDisplay = _displayBottom;

}
/*Захват крестика по касанию*/
void XPT2046_waitForCalibrationPoint(uint8_t pointIndex, uint16_t xDisplay, uint16_t yDisplay){
	_typeOfPoint = pointIndex;
	_referencePoints[pointIndex].xDisplay = xDisplay;
	_referencePoints[pointIndex].yDisplay = yDisplay;
    _isWaiting = 1;
    while(_isWaiting > 0){
    	XPT2046_Wait(100);
    }
}
/**/
void pointToScreen(){

}

uint16_t XPT2046_SingleScan(uint8_t coord){
	uint16_t res;
	XPT2046_Select();
	XPT2046_SPI_Transmit_Receive(CONTROL_STARTBIT | coord, &res);
	XPT2046_Deselect();
	return res>>3;
}
float max(float a, float b)
{
	if (a>b) {return a;} else {return b;}
}
float min(float a, float b)
{
	if (a<b) {return a;} else {return b;}
}
void XPT2046_PEN_Interrupt_Callback(){
	_isWaiting = 0;
	_xRawFiltered = 0.0;
	_yRawFiltered = 0.0;
	_z1RawFiltered = 0.0;
	_z2RawFiltered = 0.0;
	uint8_t maxScans = 1000;
	while(maxScans > 0){
	maxScans--;
	_xRaw =  XPT2046_SingleScan(CONTROL_CHANNEL_X);
	_yRaw =  XPT2046_SingleScan(CONTROL_CHANNEL_Y);
	_z1Raw = XPT2046_SingleScan(CONTROL_CHANNEL_Z1);
	_z2Raw = XPT2046_SingleScan(CONTROL_CHANNEL_Z2);
	/*усреднение точек данных*/
	_xRawFiltered = _xRawFiltered*0.98 + _xRaw	*0.02;
	_yRawFiltered = _yRawFiltered*0.98 + _yRaw	*0.02;
	_z1RawFiltered = _z1RawFiltered*0.98 + _z1Raw*0.02;
	_z2RawFiltered = _z2RawFiltered*0.98 + _z2Raw*0.02;
	_deltaX=_xRawFiltered - _xRawFilteredOld;
	_deltaY=_yRawFiltered - _yRawFilteredOld;
	_deltaZ1=_z1RawFiltered - _z1RawFilteredOld;
	_deltaZ2=_z2RawFiltered - _z2RawFilteredOld;
	if (_deltaX < 15.0 && _deltaY < 15.0) // pointFix
	{maxScans = 0;}
	}

	if (max(_z1RawFiltered,_z2RawFiltered) - min(_z1RawFiltered,_z2RawFiltered) < 200 ) //нажали не ногой
		if (_typeOfPoint == POINT_USER){
			pointToScreen();
			touch_Pressed(_xDisplay, _yDisplay);
		}
		else {
			_referencePoints[_typeOfPoint].xADC = (uint16_t) _xRawFiltered;
			_referencePoints[_typeOfPoint].yADC = (uint16_t) _yRawFiltered;
		}

}

/*определение расстояия между заявленным касанием и фактическим*/
uint16_t XPT2046_testCalibrationPoint(uint8_t pointIndex) {
	return 0;
}
