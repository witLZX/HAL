#include "usart_app.h"
#include "stdlib.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "usart.h"
#include "mydefine.h"
#include "dac_app.h"

extern WaveformInfo wave_data;
dac_waveform_t current_waveform_uart;
extern uint8_t wave_analysis_flag; // 从adc_app.c引入
extern uint8_t wave_query_type;	   // 从adc_app.c引入
uint16_t uart_rx_index = 0;
uint32_t uart_rx_ticks = 0;
uint8_t uart_rx_buffer[128] = {0};
uint8_t uart_rx_dma_buffer[128] = {0};
uint8_t uart_dma_buffer[128] = {0};
uint8_t uart_flag = 0;
struct rt_ringbuffer uart_ringbuffer;
uint8_t ringbuffer_pool[128];

int my_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
	char buffer[512];
	va_list arg;
	int len;
	// 初始化可变参数列表
	va_start(arg, format);
	len = vsnprintf(buffer, sizeof(buffer), format, arg);
	va_end(arg);
	HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)len, 0xFF);
	return len;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		uart_rx_ticks = uwTick;
		uart_rx_index++;
		HAL_UART_Receive_IT(&huart1, &uart_rx_buffer[uart_rx_index], 1);
	}
}

/**
 * @brief UART DMA接收完成或空闲事件回调函数
 * @param huart UART句柄
 * @param Size 指示在事件发生前，DMA已经成功接收了多少字节的数据
 * @retval None
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	// 1. 确认是目标串口 (USART1)
	if (huart->Instance == USART1)
	{
		// 2. 紧急停止当前的 DMA 传输 (如果还在进行中)
		//    因为空闲中断意味着发送方已经停止，防止 DMA 继续等待或出错
		HAL_UART_DMAStop(huart);

		rt_ringbuffer_put(&uart_ringbuffer, uart_rx_dma_buffer, Size);

		// 5. 清空 DMA 接收缓冲区，为下次接收做准备
		//    虽然 memcpy 只复制了 Size 个，但清空整个缓冲区更保险
		memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));

		// 6. **关键：重新启动下一次 DMA 空闲接收**
		//    必须再次调用，否则只会接收这一次
		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));

		// 7. 如果之前关闭了半满中断，可能需要在这里再次关闭 (根据需要)
		__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
	}
}

/**
 * @brief 解析串口命令函数
 * @param buffer 包含命令的缓冲区
 * @param length 缓冲区中数据的长度
 * @retval None
 */
void parse_uart_command(uint8_t *buffer, uint16_t length)
{
	// 确保缓冲区末尾有字符串结束符
	if (length < sizeof(uart_dma_buffer))
		buffer[length] = '\0';
	else
		buffer[sizeof(uart_dma_buffer) - 1] = '\0';

	// 查询指令解析
	if (strncmp((char *)buffer, "GET:", 4) == 0)
	{
		if (strncmp((char *)buffer + 4, "TYPE", 4) == 0)
		{
			wave_analysis_flag = 1;	// 触发FFT分析
			wave_query_type = 1;	// 查询波形类型
			// 在下次ADC采集完成时会更新wave_data
			my_printf(&huart1, "FFT分析中\r\n");
		}
		else if (strncmp((char *)buffer + 4, "FREQ", 4) == 0)
		{
			wave_analysis_flag = 1; // 触发FFT分析
			wave_query_type = 2;	// 查询频率
			// 在下次ADC采集完成时会更新wave_data
			my_printf(&huart1, "FFT分析中\r\n");
		}
		else if (strncmp((char *)buffer + 4, "AMP", 3) == 0)
		{
			wave_analysis_flag = 1; // 触发FFT分析
			wave_query_type = 3;	// 查询峰峰值
			// 在下次ADC采集完成时会更新wave_data
			my_printf(&huart1, "FFT分析中\r\n");
		}
		else if (strncmp((char *)buffer + 4, "ALL", 3) == 0)
		{
			wave_analysis_flag = 1; // 触发FFT分析
			wave_query_type = 0;	// 查询所有参数
			// 在下次ADC采集完成时会更新wave_data
			my_printf(&huart1, "FFT分析中\r\n");
		}
		else
		{
			my_printf(&huart1, "错误: 未知的查询参数\r\n");
		}
	}
	// 设置指令解析
	else if (strncmp((char *)buffer, "SET:", 4) == 0)
	{
		char *param_str = (char *)buffer + 4;
		char *value_str = strchr(param_str, ':');

		if (value_str == NULL)
		{
			my_printf(&huart1, "错误: 设置指令格式错误\r\n");
			return;
		}

		// 将值字符串中的冒号替换为结束符，以分隔参数名和值
		*value_str = '\0';
		value_str++; // 指向值字符串的开始

		if (strcmp(param_str, "TYPE") == 0)
		{
			// 设置波形类型
			int type = atoi(value_str);
			if (type >= 0 && type <= 2) // 检查类型是否有效
			{
				HAL_StatusTypeDef status = dac_app_set_waveform((dac_waveform_t)type);
				if (status == HAL_OK)
				{
					my_printf(&huart1, "波形类型已设置\r\n");
				}
				else
				{
					my_printf(&huart1, "设置波形类型失败\r\n");
				}
			}
			else
			{
				my_printf(&huart1, "错误: 无效的波形类型 (0-2)\r\n");
			}
		}
		else if (strcmp(param_str, "FREQ") == 0)
		{
			// 设置频率
			uint32_t freq = atoi(value_str);
			if (freq > 0) // 频率必须大于0
			{
				HAL_StatusTypeDef status = dac_app_set_frequency(freq);
				if (status == HAL_OK)
				{
					my_printf(&huart1, "频率已设置为: %dHz\r\n", freq);
				}
				else
				{
					my_printf(&huart1, "设置频率失败\r\n");
				}
			}
			else
			{
				my_printf(&huart1, "错误: 无效的频率值\r\n");
			}
		}
		else if (strcmp(param_str, "AMP") == 0)
		{
			// 设置峰峰值
			uint16_t vpp = atoi(value_str);
			// 将峰峰值转换为峰值（÷2）
			uint16_t peak_amp = vpp / 2;

			if (peak_amp > 0 && peak_amp <= (DAC_VREF_MV / 2)) // 检查峰值是否在有效范围内
			{
				HAL_StatusTypeDef status = dac_app_set_amplitude(peak_amp);
				if (status == HAL_OK)
				{
					my_printf(&huart1, "峰峰值已设置为: %dmV\r\n", vpp);
				}
				else
				{
					my_printf(&huart1, "设置峰峰值失败\r\n");
				}
			}
			else
			{
				my_printf(&huart1, "错误: 无效的峰峰值 (0-%d)\r\n", DAC_VREF_MV);
			}
		}
		else
		{
			my_printf(&huart1, "错误: 未知的设置参数\r\n");
		}
	}
	else
	{
		// 不是有效的GET或SET命令
		if (length > 1) // 避免空命令
		{
			my_printf(&huart1, "错误: 未知命令\r\n");
			my_printf(&huart1, "有效命令:\r\n");
			my_printf(&huart1, "GET:TYPE - 查询波形类型\r\n");
			my_printf(&huart1, "GET:FREQ - 查询频率\r\n");
			my_printf(&huart1, "GET:AMP - 查询峰峰值\r\n");
			my_printf(&huart1, "SET:TYPE:x - 设置波形类型(0=正弦波,1=方波,2=三角波)\r\n");
			my_printf(&huart1, "SET:FREQ:x - 设置频率(Hz)\r\n");
			my_printf(&huart1, "SET:AMP:x - 设置峰峰值(mV)\r\n");
		}
	}
}

void uart_task(void)
{
	uint16_t length;

	length = rt_ringbuffer_data_len(&uart_ringbuffer);

	if (length == 0)
		return;

	rt_ringbuffer_get(&uart_ringbuffer, uart_dma_buffer, length);

	// 调用命令解析函数
//	parse_uart_command(uart_dma_buffer, length);
	shell_process(uart_dma_buffer, length);
	// 清空接收缓冲区
	memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
}
