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
extern uint8_t wave_analysis_flag; // ��adc_app.c����
extern uint8_t wave_query_type;	   // ��adc_app.c����
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
	// ��ʼ���ɱ�����б�
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
 * @brief UART DMA������ɻ�����¼��ص�����
 * @param huart UART���
 * @param Size ָʾ���¼�����ǰ��DMA�Ѿ��ɹ������˶����ֽڵ�����
 * @retval None
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	// 1. ȷ����Ŀ�괮�� (USART1)
	if (huart->Instance == USART1)
	{
		// 2. ����ֹͣ��ǰ�� DMA ���� (������ڽ�����)
		//    ��Ϊ�����ж���ζ�ŷ��ͷ��Ѿ�ֹͣ����ֹ DMA �����ȴ������
		HAL_UART_DMAStop(huart);

		rt_ringbuffer_put(&uart_ringbuffer, uart_rx_dma_buffer, Size);

		// 5. ��� DMA ���ջ�������Ϊ�´ν�����׼��
		//    ��Ȼ memcpy ֻ������ Size �������������������������
		memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));

		// 6. **�ؼ�������������һ�� DMA ���н���**
		//    �����ٴε��ã�����ֻ�������һ��
		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));

		// 7. ���֮ǰ�ر��˰����жϣ�������Ҫ�������ٴιر� (������Ҫ)
		__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
	}
}

/**
 * @brief �������������
 * @param buffer ��������Ļ�����
 * @param length �����������ݵĳ���
 * @retval None
 */
void parse_uart_command(uint8_t *buffer, uint16_t length)
{
	// ȷ��������ĩβ���ַ���������
	if (length < sizeof(uart_dma_buffer))
		buffer[length] = '\0';
	else
		buffer[sizeof(uart_dma_buffer) - 1] = '\0';

	// ��ѯָ�����
	if (strncmp((char *)buffer, "GET:", 4) == 0)
	{
		if (strncmp((char *)buffer + 4, "TYPE", 4) == 0)
		{
			wave_analysis_flag = 1;	// ����FFT����
			wave_query_type = 1;	// ��ѯ��������
			// ���´�ADC�ɼ����ʱ�����wave_data
			my_printf(&huart1, "FFT������\r\n");
		}
		else if (strncmp((char *)buffer + 4, "FREQ", 4) == 0)
		{
			wave_analysis_flag = 1; // ����FFT����
			wave_query_type = 2;	// ��ѯƵ��
			// ���´�ADC�ɼ����ʱ�����wave_data
			my_printf(&huart1, "FFT������\r\n");
		}
		else if (strncmp((char *)buffer + 4, "AMP", 3) == 0)
		{
			wave_analysis_flag = 1; // ����FFT����
			wave_query_type = 3;	// ��ѯ���ֵ
			// ���´�ADC�ɼ����ʱ�����wave_data
			my_printf(&huart1, "FFT������\r\n");
		}
		else if (strncmp((char *)buffer + 4, "ALL", 3) == 0)
		{
			wave_analysis_flag = 1; // ����FFT����
			wave_query_type = 0;	// ��ѯ���в���
			// ���´�ADC�ɼ����ʱ�����wave_data
			my_printf(&huart1, "FFT������\r\n");
		}
		else
		{
			my_printf(&huart1, "����: δ֪�Ĳ�ѯ����\r\n");
		}
	}
	// ����ָ�����
	else if (strncmp((char *)buffer, "SET:", 4) == 0)
	{
		char *param_str = (char *)buffer + 4;
		char *value_str = strchr(param_str, ':');

		if (value_str == NULL)
		{
			my_printf(&huart1, "����: ����ָ���ʽ����\r\n");
			return;
		}

		// ��ֵ�ַ����е�ð���滻Ϊ���������Էָ���������ֵ
		*value_str = '\0';
		value_str++; // ָ��ֵ�ַ����Ŀ�ʼ

		if (strcmp(param_str, "TYPE") == 0)
		{
			// ���ò�������
			int type = atoi(value_str);
			if (type >= 0 && type <= 2) // ��������Ƿ���Ч
			{
				HAL_StatusTypeDef status = dac_app_set_waveform((dac_waveform_t)type);
				if (status == HAL_OK)
				{
					my_printf(&huart1, "��������������\r\n");
				}
				else
				{
					my_printf(&huart1, "���ò�������ʧ��\r\n");
				}
			}
			else
			{
				my_printf(&huart1, "����: ��Ч�Ĳ������� (0-2)\r\n");
			}
		}
		else if (strcmp(param_str, "FREQ") == 0)
		{
			// ����Ƶ��
			uint32_t freq = atoi(value_str);
			if (freq > 0) // Ƶ�ʱ������0
			{
				HAL_StatusTypeDef status = dac_app_set_frequency(freq);
				if (status == HAL_OK)
				{
					my_printf(&huart1, "Ƶ��������Ϊ: %dHz\r\n", freq);
				}
				else
				{
					my_printf(&huart1, "����Ƶ��ʧ��\r\n");
				}
			}
			else
			{
				my_printf(&huart1, "����: ��Ч��Ƶ��ֵ\r\n");
			}
		}
		else if (strcmp(param_str, "AMP") == 0)
		{
			// ���÷��ֵ
			uint16_t vpp = atoi(value_str);
			// �����ֵת��Ϊ��ֵ����2��
			uint16_t peak_amp = vpp / 2;

			if (peak_amp > 0 && peak_amp <= (DAC_VREF_MV / 2)) // ����ֵ�Ƿ�����Ч��Χ��
			{
				HAL_StatusTypeDef status = dac_app_set_amplitude(peak_amp);
				if (status == HAL_OK)
				{
					my_printf(&huart1, "���ֵ������Ϊ: %dmV\r\n", vpp);
				}
				else
				{
					my_printf(&huart1, "���÷��ֵʧ��\r\n");
				}
			}
			else
			{
				my_printf(&huart1, "����: ��Ч�ķ��ֵ (0-%d)\r\n", DAC_VREF_MV);
			}
		}
		else
		{
			my_printf(&huart1, "����: δ֪�����ò���\r\n");
		}
	}
	else
	{
		// ������Ч��GET��SET����
		if (length > 1) // ���������
		{
			my_printf(&huart1, "����: δ֪����\r\n");
			my_printf(&huart1, "��Ч����:\r\n");
			my_printf(&huart1, "GET:TYPE - ��ѯ��������\r\n");
			my_printf(&huart1, "GET:FREQ - ��ѯƵ��\r\n");
			my_printf(&huart1, "GET:AMP - ��ѯ���ֵ\r\n");
			my_printf(&huart1, "SET:TYPE:x - ���ò�������(0=���Ҳ�,1=����,2=���ǲ�)\r\n");
			my_printf(&huart1, "SET:FREQ:x - ����Ƶ��(Hz)\r\n");
			my_printf(&huart1, "SET:AMP:x - ���÷��ֵ(mV)\r\n");
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

	// ���������������
//	parse_uart_command(uart_dma_buffer, length);
	shell_process(uart_dma_buffer, length);
	// ��ս��ջ�����
	memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
}
