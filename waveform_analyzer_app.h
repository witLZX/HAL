#ifndef __Waveform_Analyzer_APP_H
#define __Waveform_Analyzer_APP_H

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

// �������Ͷ��壨ʹ��ö�����͸�������
typedef enum
{
    ADC_WAVEFORM_DC = 0,       // ֱ���ź�
    ADC_WAVEFORM_SINE = 1,     // ���Ҳ�
    ADC_WAVEFORM_SQUARE = 2,   // ����
    ADC_WAVEFORM_TRIANGLE = 3, // ���ǲ�
    ADC_WAVEFORM_UNKNOWN = 255 // δ֪����
} ADC_WaveformType;

// г��������Ϣ�ṹ��
typedef struct
{
    float frequency;    // ����Ƶ��
    float amplitude;    // ��������
    float phase;        // ������λ
    float relative_amp; // ����ڻ����ķ��ȱ�
} HarmonicComponent;

// ��չ�Ĳ�����Ϣ�ṹ�壨����г��������Ϣ��
typedef struct
{
    ADC_WaveformType waveform_type;   // ��������ö��
    float frequency;                  // ����Ƶ�ʣ���λHz
    float vpp;                        // ���ֵ����λV
    float mean;                       // ��ֵ����λV
    float rms;                        // ��Чֵ����λV
    float phase;                      // ������λ����λ����
    HarmonicComponent third_harmonic; // ����г����Ϣ
    HarmonicComponent fifth_harmonic; // ���г����Ϣ
} WaveformInfo;

// ��������
void My_FFT_Init(void);

// Ƶ��ӳ�亯��
float Map_Input_To_FFT_Frequency(float input_frequency);
float Map_FFT_To_Input_Frequency(float fft_frequency);

// �������η�������
float Get_Waveform_Vpp(uint32_t *adc_val_buffer_f, float *mean, float *rms);
float Get_Waveform_Frequency(uint32_t *adc_val_buffer_f);
ADC_WaveformType Get_Waveform_Type(uint32_t *adc_val_buffer_f);

// FFT��غ���
void Perform_FFT(uint32_t *adc_val_buffer_f);
ADC_WaveformType Analyze_Frequency_And_Type(uint32_t *adc_val_buffer_f, float *signal_frequency);

// ��λ���㺯��
float Get_Waveform_Phase(uint32_t *adc_val_buffer_f, float frequency);
float Get_Waveform_Phase_ZeroCrossing(uint32_t *adc_val_buffer_f, float frequency);
float Calculate_Phase_Difference(float phase1, float phase2);
float Get_Phase_Difference(uint32_t *adc_val_buffer1, uint32_t *adc_val_buffer2, float frequency);

// г����������
void Analyze_Harmonics(uint32_t *adc_val_buffer_f, WaveformInfo *waveform_info);
float Get_Component_Phase(float *fft_buffer, int component_idx);

// ��������
const char *GetWaveformTypeString(ADC_WaveformType waveform);

// �ۺϽӿ�
WaveformInfo Get_Waveform_Info(uint32_t *adc_val_buffer_f);

#endif // Waveform_Analyzer_APP_H
