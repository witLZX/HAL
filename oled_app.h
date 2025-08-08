#ifndef __OLED_APP_H__
#define __OLED_APP_H__

#include "mydefine.h"

int oled_printf(uint8_t x, uint8_t y, const char *format, ...);
void oled_task(void);
uint8_t u8g2_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
void OLED_SendBuff(uint8_t buff[4][128]);
void PIDMenu_Init(void);
#endif


