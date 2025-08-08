#ifndef __USART_APP_H__
#define __USART_APP_H__

#include "mydefine.h"

int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
void uart_task(void);

#endif


