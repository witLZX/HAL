#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "stdint.h"
#include "stdlib.h"

#include "WouoUI.h"    
#include "WouoUI_user.h" 
#include "u8g2.h"
#include "i2c.h"
#include "main.h"
#include "usart.h"
#include "math.h"
#include "adc.h"
#include "tim.h"
#include "dac.h"
#include "oled.h"
#include "lfs.h"
#include "lfs_port.h"
#include "gd25qxx.h"
#include "scheduler.h"
#include "ringbuffer.h"
#include "arm_math.h"

#include "oled_app.h"
#include "adc_app.h"
#include "dac_app.h"
#include "led_app.h"
#include "btn_app.h"
#include "flash_app.h"
#include "usart_app.h"
#include "shell_app.h"
#include "waveform_analyzer_app.h"

extern uint16_t uart_rx_index;
extern uint32_t uart_rx_ticks;
extern uint8_t uart_rx_buffer[128];
extern uint8_t uart_rx_dma_buffer[128];
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern struct rt_ringbuffer uart_ringbuffer;
extern uint8_t ringbuffer_pool[128];
extern uint8_t uart_send_flag;
extern uint8_t wave_analysis_flag;
extern u8g2_t u8g2; // ȫ�� u8g2 ʵ��
extern struct lfs_config cfg;
extern lfs_t lfs;
