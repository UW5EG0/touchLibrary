/*
 * XPT2046.h
 *
 *  Created on: Jan 10, 2022
 *      Author: user
 */

#ifndef XPT2046_INC_XPT2046_H_
#define XPT2046_INC_XPT2046_H_

/* Includes ------------------------------------------------------------------*/
#include "XPT2046.c"
#include <stdint.h>

/* Private macros ------------------------------------------------------------*/


/* functions ---------------------------------------------------------*/
void XPT2046_init(uint16_t displayWidth, uint16_t displayHeight, int16_t displayLeft, int16_t displayBottom);
void XPT2046_clearCalibrationData();
void XPT2046_directSetCalibrationPoint(uint8_t pointIndex, int16_t xDisplay, int16_t yDisplay, uint16_t xADC, uint16_t yADC);
uint8_t XPT2046_updateCalibrationParameters();
uint32_t XPT2046_GetTouchPressDuration();
void XPT2046_PEN_DOWN_Interrupt_Callback();
void XPT2046_PEN_UP_Interrupt_Callback();
#endif /* XPT2046_INC_XPT2046_H_ */
