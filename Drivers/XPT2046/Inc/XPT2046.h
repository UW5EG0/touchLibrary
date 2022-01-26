/*
 * XPT2046.h
 *
 *  Created on: Jan 10, 2022
 *      Author: user
 */

#ifndef XPT2046_INC_XPT2046_H_
#define XPT2046_INC_XPT2046_H_

/* Includes ------------------------------------------------------------------*/
#include "../Src/XPT2046.c"


/* Private macros ------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/*Методы, что надо реализовать на стороне юзера*/
void XPT2046_SPI_send(uint8_t data);
void XPT2046_Select();
void XPT2046_Deselect();
void XPT2046_SPI_Transmit_Receive(uint8_t data_in, uint16_t * data_out);
/*блокирующая функция - нужна для передачи управления другому потоку при ожидании касания */
void XPT2046_Wait(uint32_t timeout);
/*колбеки для реакции на касание и отпускание тача */
void touch_Pressed(uint16_t x, uint16_t y);
void touch_Released(uint32_t duration);

/* Private functions ---------------------------------------------------------*/
void XPT2046_init(uint16_t displayWidth, uint16_t displayHeight, int16_t displayLeft, int16_t displayBottom);
void XPT2046_clearCalibrationData();
uint32_t XPT2046_GetTouchPressDuration();
void XPT2046_PEN_Interrupt_Callback();

#endif /* XPT2046_INC_XPT2046_H_ */
