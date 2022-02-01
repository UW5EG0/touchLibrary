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
#define XPT2046_SWAP_XY
#define CONTROL_STARTBIT			0b10000000
#ifndef XPT2046_SWAP_XY
#define CONTROL_CHANNEL_X			0b00010000
#define CONTROL_CHANNEL_Y			0b01010000
#else
#define CONTROL_CHANNEL_Y			0b00010000
#define CONTROL_CHANNEL_X			0b01010000
#endif
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

#define _XTLT (_referencePoints[POINT_TOPLEFT].xADC)  /*X of Top Left Touch*/
#define _XTLS (_referencePoints[POINT_TOPLEFT].xDisplay)  /*X of Top Left Screen*/
#define _YTLT (_referencePoints[POINT_TOPLEFT].yADC)  /*Y of Top Left Touch*/
#define _YTLS (_referencePoints[POINT_TOPLEFT].yDisplay)  /*Y of Top Left Screen*/

#define _XTRT (_referencePoints[POINT_TOPRIGHT].xADC)  /*X of Top Right Touch*/
#define _XTRS (_referencePoints[POINT_TOPRIGHT].xDisplay)  /*X of Top Right Screen*/
#define _YTRT (_referencePoints[POINT_TOPRIGHT].yADC)  /*Y of Top Right Touch*/
#define _YTRS (_referencePoints[POINT_TOPRIGHT].yDisplay)  /*Y of Top Right Screen*/

#define _XBLT (_referencePoints[POINT_BOTTOMLEFT].xADC)  /*X of Bottom Left Touch*/
#define _XBLS (_referencePoints[POINT_BOTTOMLEFT].xDisplay)  /*X of Bottom Left Screen*/
#define _YBLT (_referencePoints[POINT_BOTTOMLEFT].yADC)  /*Y of Bottom Left Touch*/
#define _YBLS (_referencePoints[POINT_BOTTOMLEFT].yDisplay)  /*Y of Bottom Left Screen*/

#define _XBRT (_referencePoints[POINT_BOTTOMRIGHT].xADC)  /*X of Bottom Right Touch*/
#define _XBRS (_referencePoints[POINT_BOTTOMRIGHT].xDisplay)  /*X of Bottom Right Screen*/
#define _YBRT (_referencePoints[POINT_BOTTOMRIGHT].yADC)  /*Y of Bottom Right Touch*/
#define _YBRS (_referencePoints[POINT_BOTTOMRIGHT].yDisplay)  /*Y of Bottom Right Screen*/

#define _XCT (_referencePoints[POINT_CENTER].xADC)  /*X of Center Touch*/
#define _XCS (_referencePoints[POINT_CENTER].xDisplay)  /*X of Center Screen*/
#define _YCT (_referencePoints[POINT_CENTER].yADC)  /*Y of Center Touch*/
#define _YCS (_referencePoints[POINT_CENTER].yDisplay)  /*Y of Center Screen*/

#define XPT2046_OK 0
#define XPT2046_ERR 1
#define NO_PRESSURE_CHECK 1
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
float _xStep;
float _yStep;
int16_t _xRawOnZeroPoint;
int16_t _yRawOnZeroPoint;

int16_t _xDisplay;
int16_t _yDisplay;
uint8_t _isWaiting = 0;
int8_t _typeOfPoint;

//
uint32_t _startTouchTickMS;
uint32_t _endTouchTickMS;
uint32_t _lastTouchDuration;

ref_point_t _referencePoints[5]; //контрольные точки для калибровки

/* Private constants ---------------------------------------------------------*/
typedef enum {
	POINT_CENTER,
	POINT_TOPLEFT,
	POINT_TOPRIGHT,
	POINT_BOTTOMLEFT,
	POINT_BOTTOMRIGHT} reference_points;

/* Private function prototypes -----------------------------------------------*/
	/*Методы, что надо реализовать на стороне юзера*/
	void XPT2046_SPI_send(uint8_t data);
	void XPT2046_Select();
	void XPT2046_Deselect();
	uint32_t XPT2046_GetTick();
	void XPT2046_SPI_Transmit_Receive(uint8_t data_in, uint16_t *data_out);

	/*блокирующая функция - нужна для передачи управления другому потоку при ожидании касания */
	void XPT2046_Wait(uint32_t timeout);
	/*колбеки для реакции на касание и отпускание тача */
	void touch_Pressed(uint16_t x, uint16_t y);
	void touch_Released(uint32_t duration);
	void XPT2046_Enable_Interrupt();
	void XPT2046_Disable_Interrupt();
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
	XPT2046_Enable_Interrupt();
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
	_referencePoints[POINT_TOPLEFT].yADC = 4095;
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

	_referencePoints[POINT_BOTTOMRIGHT].xADC = 4095;
	_referencePoints[POINT_BOTTOMRIGHT].yADC = 0;
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
/*Пересчет координат */
void XPT2046_pointToScreen(){
	_xDisplay = _displayLeft + (_xRawFiltered-_xRawOnZeroPoint)/_xStep;
	_yDisplay = _displayBottom + (_yRawFiltered-_yRawOnZeroPoint)/_yStep;
}


uint16_t XPT2046_SingleScan(uint8_t coord){
	uint16_t res;
	XPT2046_Select();
	XPT2046_SPI_Transmit_Receive(CONTROL_STARTBIT |coord, &res);
	XPT2046_Deselect();
	res >>= 3;
	return res;
}
float max(float a, float b)
{
	if (a>b) {return a;} else {return b;}
}
float min(float a, float b)
{
	if (a<b) {return a;} else {return b;}
}
/*Сработка при нажатии на тач*/
void XPT2046_PEN_DOWN_Interrupt_Callback(){
	XPT2046_Disable_Interrupt();
	_startTouchTickMS = XPT2046_GetTick();
	_isWaiting = 0;
	_xRawFiltered = 0.0;
	_yRawFiltered = 0.0;
	_z1RawFiltered = 0.0;
	_z2RawFiltered = 0.0;
	uint16_t maxScans = 100;
	while(maxScans > 0){
	maxScans--;
	_xRaw =  XPT2046_SingleScan(CONTROL_CHANNEL_X);
	_yRaw =  XPT2046_SingleScan(CONTROL_CHANNEL_Y);
	_z1Raw = XPT2046_SingleScan(CONTROL_CHANNEL_Z1);
	_z2Raw = XPT2046_SingleScan(CONTROL_CHANNEL_Z2);
	if (_xRaw > 0 && _yRaw > 0 && _xRaw < 4096 && _yRaw < 4096)
	{/*усреднение точек данных*/

	_xRawFiltered = _xRawFiltered*0.98 + _xRaw	*0.02;
	_yRawFiltered = _yRawFiltered*0.98 + _yRaw	*0.02;
	_z1RawFiltered = _z1RawFiltered*0.75 + _z1Raw*0.25;
	_z2RawFiltered = _z2RawFiltered*0.75 + _z2Raw*0.25;
	_deltaX=_xRawFiltered - _xRawFilteredOld;
	_deltaY=_yRawFiltered - _yRawFilteredOld;
	_deltaZ1=_z1RawFiltered - _z1RawFilteredOld;
	_deltaZ2=_z2RawFiltered - _z2RawFilteredOld;
	if (_deltaX < 1.0 && _deltaY < 1.0) // pointFix
	{maxScans = 0;
		}
	}
	}
	if (((max(_z1RawFiltered,_z2RawFiltered) - min(_z1RawFiltered,_z2RawFiltered) < 200)
		&& (max(_z1RawFiltered,_z2RawFiltered) - min(_z1RawFiltered,_z2RawFiltered) > 3))
		|| NO_PRESSURE_CHECK) //нажали не ногой и не отпустили
		{
		if (_typeOfPoint == POINT_USER){
			XPT2046_pointToScreen();
			touch_Pressed(_xDisplay, _yDisplay);
		}
		else {
			_referencePoints[_typeOfPoint].xADC = (uint16_t) _xRawFiltered;
			_referencePoints[_typeOfPoint].yADC = (uint16_t) _yRawFiltered;
		}
	}
	XPT2046_Enable_Interrupt();
}
/*реакция на отпускание тача*/
void XPT2046_PEN_UP_Interrupt_Callback(){
	_endTouchTickMS = XPT2046_GetTick();
	_lastTouchDuration = _endTouchTickMS - _startTouchTickMS;
	touch_Released(_lastTouchDuration);
}
/*Принудительная установка точек калибровки*/
void XPT2046_directSetCalibrationPoint(uint8_t pointIndex, int16_t xDisplay, int16_t yDisplay, uint16_t xADC, uint16_t yADC){
	if (pointIndex >=POINT_CENTER && pointIndex <= POINT_BOTTOMRIGHT){
		_referencePoints[pointIndex].xDisplay = xDisplay;
		_referencePoints[pointIndex].yDisplay = yDisplay;
		_referencePoints[pointIndex].xADC = xADC;
		_referencePoints[pointIndex].yADC = yADC;
	}
}
/*На основании данных калибровочных точек определяем коэфициенты*/
uint8_t XPT2046_updateCalibrationParameters(){
	_xStep = (float)((_XBRT -_XTLT)+(_XTRT - _XBLT))/ (float)((_XBRS - _XTLS)+(_XTRS - _XBLS));
	_yStep = (float)((_YTLT - _YBRT)+(_YTRT - _YBLT))/ (float)((_YTLS - _YBRS)+(_YTRS - _YBLS));
	_xRawOnZeroPoint = (_XCT-(_xStep * (_XCS-_displayLeft)));
	_yRawOnZeroPoint = (_YCT-(_yStep * (_YCS-_displayBottom)));

	return XPT2046_OK;
}
uint32_t XPT2046_GetTouchPressDuration(){
  return XPT2046_GetTick() - _startTouchTickMS;
}
/*определение расстояия между заявленным касанием и фактическим*/
uint16_t XPT2046_testCalibrationPoint(uint8_t pointIndex) {
	return 0;
}
