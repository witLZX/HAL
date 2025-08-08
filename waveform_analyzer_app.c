#include "waveform_analyzer_app.h"
#include <float.h>

#define FFT_LENGTH 1024 // �����adc����������Ҫ����һ��
arm_cfft_radix4_instance_f32 scfft;
float FFT_InputBuf[FFT_LENGTH * 2];
float FFT_OutputBuf[FFT_LENGTH];

/**
 * @brief ����Ƶ�ʵ�FFTƵ�ʵ�ӳ�亯��
 * @param input_frequency ����Ƶ��(Hz)
 * @return ӳ����FFTƵ��(Hz)
 */
float Map_Input_To_FFT_Frequency(float input_frequency)
{
    // ���ڹ��ɵ������ͳ���ӳ��
    if (input_frequency <= 2600.0f)
    {
        return input_frequency; // ����Ϊ1.0
    }
    else if (input_frequency <= 6100.0f)
    {
        return input_frequency * 2.0f; // ����Ϊ2.0
    }
    else if (input_frequency <= 8100.0f)
    {
        return input_frequency * 3.0f; // ����Ϊ3.0
    }
    else if (input_frequency <= 11100.0f)
    {
        return input_frequency * 4.0f; // ����Ϊ4.0
    }
    else if (input_frequency <= 14100.0f)
    {
        return input_frequency * 5.0f; // ����Ϊ5.0
    }
    else if (input_frequency <= 17100.0f)
    {
        return input_frequency * 6.0f; // ����Ϊ6.0
    }
    else if (input_frequency <= 19600.0f)
    {
        return input_frequency * 7.0f; // ����Ϊ7.0
    }
    else if (input_frequency <= 21600.0f)
    {
        return input_frequency * 8.0f; // ����Ϊ8.0
    }
    else if (input_frequency <= 25100.0f)
    {
        return input_frequency * 9.0f; // ����Ϊ9.0
    }
    else if (input_frequency <= 26600.0f)
    {
        return input_frequency * 10.0f; // ����Ϊ10.0
    }
    else if (input_frequency <= 29600.0f)
    {
        return input_frequency * 11.0f; // ����Ϊ11.0
    }
    else if (input_frequency <= 32100.0f)
    {
        return input_frequency * 12.0f; // ����Ϊ12.0
    }
    else
    {
        return input_frequency * 13.0f; // ����Ϊ13.0�����
    }
}

/**
 * @brief ��FFTƵ�ʷ�������Ƶ��
 * @param fft_frequency FFTƵ��(Hz)
 * @return ���Ƶ�����Ƶ��(Hz)
 */
float Map_FFT_To_Input_Frequency(float fft_frequency)
{
    // ���������
    const float breaks[] = {2600.0f, 6100.0f, 8100.0f, 11100.0f, 14100.0f,
                            17100.0f, 19600.0f, 21600.0f, 25100.0f, 26600.0f,
                            29600.0f, 32100.0f};
    // ��Ӧ�ĳ���
    const float dividers[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                              9.0f, 10.0f, 11.0f, 12.0f, 13.0f};

    // ������������������Ӧ�ı���
    float last_break_fft = breaks[sizeof(breaks) / sizeof(breaks[0]) - 1] * dividers[sizeof(breaks) / sizeof(breaks[0])];

    // ���FFTƵ�ʴ������������Ӧ��FFTƵ��
    if (fft_frequency > last_break_fft)
    {
        return fft_frequency / dividers[sizeof(dividers) / sizeof(dividers[0]) - 1];
    }

    // ͨ�����ֲ���ȷ��Ƶ�ʷ�Χ
    for (int i = 0; i < sizeof(breaks) / sizeof(breaks[0]); i++)
    {
        float break_fft = breaks[i] * dividers[i];
        float next_break_fft = (i < sizeof(breaks) / sizeof(breaks[0]) - 1) ? breaks[i + 1] * dividers[i + 1] : FLT_MAX;

        if (fft_frequency <= break_fft)
        {
            return fft_frequency / dividers[i];
        }
        else if (fft_frequency < next_break_fft)
        {
            return fft_frequency / dividers[i + 1];
        }
    }

    // Ĭ�����
    return fft_frequency / 13.0f;
}

/**
 * @brief ��ʼ��FFTģ��
 */
void My_FFT_Init(void)
{
    arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH, 0, 1); // ��ʼ��scfft�ṹ��,�趨FFT����
}

/**
 * @brief ���㲨�εķ��ֵ����ֵ����Чֵ
 * @param adc_val_buffer_f ADC����������
 * @param mean ָ��洢��ֵ�ı�����ָ��
 * @param rms ָ��洢��Чֵ�ı�����ָ��
 * @return ���ֵ�����أ�
 */
float Get_Waveform_Vpp(uint32_t *adc_val_buffer_f, float *mean, float *rms)
{
    // ��ʼ��Ϊ��ֵ
    float min_val = 3.3f; // ��ʼ��Ϊ�����ܵ�ѹ
    float max_val = 0.0f; // ��ʼ��Ϊ��С���ܵ�ѹ

    float sum = 0.0f;         // ���ڼ����ֵ
    float sum_squares = 0.0f; // ���ڼ�����Чֵ
    float voltage;

    // �������в������ҳ����ֵ����Сֵ���������ֵ����Чֵ
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        voltage = (float)adc_val_buffer_f[i] / 4096.0f * 3.3f;

        // ���ֵ���� - ����������Сֵ
        if (voltage > max_val)
            max_val = voltage;
        if (voltage < min_val)
            min_val = voltage;

        // ��ֵ���� - �ۼ����е�ѹֵ
        sum += voltage;

        // ��Чֵ���� - �ۼ����е�ѹֵ��ƽ��
        sum_squares += voltage * voltage;
    }

    // �����ֵ
    *mean = sum / (float)FFT_LENGTH;

    // ������Чֵ��RMS��
    *rms = sqrtf(sum_squares / (float)FFT_LENGTH);

    // ���㲢���ط��ֵ
    return max_val - min_val;
}

/**
 * @brief ִ��FFT�任�����FFT���������
 * @param adc_val_buffer_f ADC����������
 */
void Perform_FFT(uint32_t *adc_val_buffer_f)
{
    // ��ADCֵת��Ϊ�������ݲ����FFT���뻺����
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[2 * i] = (float)adc_val_buffer_f[i] / 4096.0f * 3.3f; // ʵ��
        FFT_InputBuf[2 * i + 1] = 0;                                       // �鲿
    }

    // ִ��FFT����
    arm_cfft_radix4_f32(&scfft, FFT_InputBuf);                  // FFT���㣨��4��
    arm_cmplx_mag_f32(FFT_InputBuf, FFT_OutputBuf, FFT_LENGTH); // ȡģ�÷�ֵ
}

/**
 * @brief ��ȡFFT������ض�Ƶ�ʷ�������λ
 * @param fft_buffer FFT������������
 * @param component_idx ��������
 * @return ��λ�ǣ������ƣ���ΧΪ-PI��PI��
 */
float Get_Component_Phase(float *fft_buffer, int component_idx)
{
    // ��FFT����л�ȡʵ�����鲿
    float real_part = fft_buffer[2 * component_idx];
    float imag_part = fft_buffer[2 * component_idx + 1];

    // ������λ�ǣ������ƣ�
    float phase = atan2f(imag_part, real_part);

    return phase;
}

/**
 * @brief �����źŵ���λ��
 * @param adc_val_buffer_f ADC����������
 * @param frequency �ź�Ƶ��
 * @return ��λ�ǣ������ƣ���ΧΪ-PI��PI��
 */
float Get_Waveform_Phase(uint32_t *adc_val_buffer_f, float frequency)
{
    // �����DC�źŻ�δ֪�źţ���λ������
    if (frequency <= 0.0f)
    {
        return 0.0f;
    }

    // ִ��FFT�任
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[2 * i] = (float)adc_val_buffer_f[i] / 4096.0f * 3.3f; // ʵ��
        FFT_InputBuf[2 * i + 1] = 0;                                       // �鲿
    }

    arm_cfft_radix4_f32(&scfft, FFT_InputBuf); // FFT����

    // ������Ƶ��ӳ�䵽FFTƵ��
    float fft_frequency = Map_Input_To_FFT_Frequency(frequency);

    // �����Ӧ�ڻ�Ƶ��FFT�������
    float sampling_interval_us = dac_app_get_adc_sampling_interval_us();
    float sampling_frequency = 1000000.0f / sampling_interval_us;
    int fundamental_idx = (int)(fft_frequency * FFT_LENGTH / sampling_frequency + 0.5f);

    // ������������Ч��Χ��
    if (fundamental_idx <= 0 || fundamental_idx >= FFT_LENGTH / 2)
    {
        return 0.0f;
    }

    return Get_Component_Phase(FFT_InputBuf, fundamental_idx);
}

/**
 * @brief ʹ�ù����������ź���λ
 * @param adc_val_buffer_f ADC����������
 * @param frequency �ź�Ƶ��
 * @return ��λ�ǣ������ƣ���ΧΪ0��2PI��
 */
float Get_Waveform_Phase_ZeroCrossing(uint32_t *adc_val_buffer_f, float frequency)
{
    // �����DC�źŻ�δ֪�źţ���λ������
    if (frequency <= 0.0f)
    {
        return 0.0f;
    }

    float sampling_interval_us = dac_app_get_adc_sampling_interval_us();
    float sampling_frequency = 1000000.0f / sampling_interval_us;

    // ����ƽ��ֵ
    float mean = 0.0f;
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        mean += (float)adc_val_buffer_f[i] / 4096.0f * 3.3f;
    }
    mean /= (float)FFT_LENGTH;

    // Ѱ�ҵ�һ������㣨�Ӹ�������
    int zero_crossing_idx = -1;
    for (int i = 1; i < FFT_LENGTH; i++)
    {
        float current_val = (float)adc_val_buffer_f[i] / 4096.0f * 3.3f - mean;
        float prev_val = (float)adc_val_buffer_f[i - 1] / 4096.0f * 3.3f - mean;

        if (prev_val < 0.0f && current_val >= 0.0f)
        {
            // �ҵ������
            zero_crossing_idx = i;
            break;
        }
    }

    // ���û���ҵ������
    if (zero_crossing_idx < 0)
    {
        return 0.0f;
    }

    // ��ȷ�����λ�ã����Բ�ֵ��
    float prev_val = (float)adc_val_buffer_f[zero_crossing_idx - 1] / 4096.0f * 3.3f - mean;
    float current_val = (float)adc_val_buffer_f[zero_crossing_idx] / 4096.0f * 3.3f - mean;

    // ��������ľ�ȷλ��
    float fraction = -prev_val / (current_val - prev_val);
    float exact_crossing = (float)(zero_crossing_idx - 1) + fraction;

    // ��ȡ����Ƶ�ʶ�Ӧ��FFTƵ��
    float fft_frequency = Map_Input_To_FFT_Frequency(frequency);

    // �������ڣ�����������
    float samples_per_period = sampling_frequency / fft_frequency;

    // ������λ�ǣ�0��2PI��
    float phase = 2.0f * PI * exact_crossing / samples_per_period;

    // ����λ������0��2PI��Χ��
    while (phase < 0.0f)
        phase += 2.0f * PI;
    while (phase >= 2.0f * PI)
        phase -= 2.0f * PI;

    return phase;
}

/**
 * @brief ���������ź�֮�����λ��
 * @param phase1 ��һ���źŵ���λ
 * @param phase2 �ڶ����źŵ���λ
 * @return ��λ������ƣ���ΧΪ-PI��PI��
 */
float Calculate_Phase_Difference(float phase1, float phase2)
{
    float phase_diff = phase1 - phase2;

    // ����λ��������-PI��PI��Χ��
    while (phase_diff > PI)
        phase_diff -= 2.0f * PI;
    while (phase_diff <= -PI)
        phase_diff += 2.0f * PI;

    return phase_diff;
}

/**
 * @brief ��ȡ���ε�Ƶ��
 * @param adc_val_buffer_f ADC����������
 * @return Ƶ�ʣ�Hz��
 */
float Get_Waveform_Frequency(uint32_t *adc_val_buffer_f)
{
    float sampling_interval_us = dac_app_get_adc_sampling_interval_us(); // ��ȡ���������΢�룩
    float sampling_frequency = 1000000.0f / sampling_interval_us;        // �������Ƶ�ʣ�Hz��

    // ִ��FFT�任
    Perform_FFT(adc_val_buffer_f);

    // ���һ�Ƶ������ֱ��������
    float max_harmonic = 0;   // ���г������
    int max_harmonic_idx = 0; // ���г����������

    for (int i = 1; i < FFT_LENGTH / 2; i++) // ֻ�迼��ǰһ��Ƶ�ף�NyquistƵ�ʣ�
    {
        if (FFT_OutputBuf[i] > max_harmonic)
        {
            max_harmonic = FFT_OutputBuf[i];
            max_harmonic_idx = i;
        }
    }

    // ����FFTƵ�ʣ�Hz��
    // Ƶ�ʼ��㹫ʽ: freq = idx * sampling_freq / FFT_LENGTH
    float fft_frequency = (float)max_harmonic_idx * sampling_frequency / (float)FFT_LENGTH;

    // ���ֱ���ź�
    if (FFT_OutputBuf[0] > max_harmonic * 5.0f)
    {
        return 0.0f; // �����ֱ���źţ�����0Hz
    }

    // ��FFTƵ��ӳ�������Ƶ��
    return Map_FFT_To_Input_Frequency(fft_frequency);
}

/**
 * @brief �������ε�г���ɷ�
 * @param adc_val_buffer_f ADC����������
 * @param waveform_info ָ������Ϣ�ṹ���ָ��
 */
void Analyze_Harmonics(uint32_t *adc_val_buffer_f, WaveformInfo *waveform_info)
{
    float sampling_interval_us = dac_app_get_adc_sampling_interval_us();
    float sampling_frequency = 1000000.0f / sampling_interval_us;

    // ȷ������Ч�Ļ�Ƶ
    if (waveform_info->frequency <= 0.0f)
    {
        // ��ʼ��г����ϢΪ0
        waveform_info->third_harmonic.frequency = 0.0f;
        waveform_info->third_harmonic.amplitude = 0.0f;
        waveform_info->third_harmonic.phase = 0.0f;
        waveform_info->third_harmonic.relative_amp = 0.0f;

        waveform_info->fifth_harmonic.frequency = 0.0f;
        waveform_info->fifth_harmonic.amplitude = 0.0f;
        waveform_info->fifth_harmonic.phase = 0.0f;
        waveform_info->fifth_harmonic.relative_amp = 0.0f;
        return;
    }

    // ִ��FFT�任
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[2 * i] = (float)adc_val_buffer_f[i] / 4096.0f * 3.3f; // ʵ��
        FFT_InputBuf[2 * i + 1] = 0;                                       // �鲿
    }

    arm_cfft_radix4_f32(&scfft, FFT_InputBuf); // FFT����

    // ���������
    float magnitude_spectrum[FFT_LENGTH];
    arm_cmplx_mag_f32(FFT_InputBuf, magnitude_spectrum, FFT_LENGTH);

    // ������Ƶ��ӳ�䵽FFTƵ��
    float fft_frequency = Map_Input_To_FFT_Frequency(waveform_info->frequency);

    // �����Ƶ��Ӧ��FFT����
    int fundamental_idx = (int)(fft_frequency * FFT_LENGTH / sampling_frequency + 0.5f);

    // ��ȡ�������Ⱥ���λ
    float fundamental_amp = magnitude_spectrum[fundamental_idx];
    float fundamental_phase = Get_Component_Phase(FFT_InputBuf, fundamental_idx);

    // ��������г�������ͷ�Χ
    int expected_third_harmonic_idx = 3 * fundamental_idx;
    int third_search_start = expected_third_harmonic_idx - fundamental_idx / 4;
    int third_search_end = expected_third_harmonic_idx + fundamental_idx / 4;

    // ȷ����������Ч��Χ��
    if (third_search_start < fundamental_idx)
        third_search_start = fundamental_idx + 1;
    if (third_search_end >= FFT_LENGTH / 2)
        third_search_end = FFT_LENGTH / 2 - 1;

    // Ѱ������г����ֵ
    float third_harmonic_amp = 0.0f;
    int third_harmonic_idx = 0;

    for (int i = third_search_start; i <= third_search_end; i++)
    {
        if (magnitude_spectrum[i] > third_harmonic_amp)
        {
            third_harmonic_amp = magnitude_spectrum[i];
            third_harmonic_idx = i;
        }
    }

    // �������г�������ͷ�Χ
    int expected_fifth_harmonic_idx = 5 * fundamental_idx;
    int fifth_search_start = expected_fifth_harmonic_idx - fundamental_idx / 4;
    int fifth_search_end = expected_fifth_harmonic_idx + fundamental_idx / 4;

    // ȷ����������Ч��Χ��
    if (fifth_search_start < third_harmonic_idx + 1)
        fifth_search_start = third_harmonic_idx + 1;
    if (fifth_search_end >= FFT_LENGTH / 2)
        fifth_search_end = FFT_LENGTH / 2 - 1;

    // Ѱ�����г����ֵ
    float fifth_harmonic_amp = 0.0f;
    int fifth_harmonic_idx = 0;

    for (int i = fifth_search_start; i <= fifth_search_end; i++)
    {
        if (magnitude_spectrum[i] > fifth_harmonic_amp)
        {
            fifth_harmonic_amp = magnitude_spectrum[i];
            fifth_harmonic_idx = i;
        }
    }

    // ���Է���̫С��г����С�ڻ������ȵ�5%��
    if (third_harmonic_amp < fundamental_amp * 0.05f)
    {
        third_harmonic_amp = 0.0f;
        third_harmonic_idx = 0;
    }

    if (fifth_harmonic_amp < fundamental_amp * 0.05f)
    {
        fifth_harmonic_amp = 0.0f;
        fifth_harmonic_idx = 0;
    }

    // ����г������λ
    float third_harmonic_phase = (third_harmonic_idx > 0) ? Get_Component_Phase(FFT_InputBuf, third_harmonic_idx) : 0.0f;

    float fifth_harmonic_phase = (fifth_harmonic_idx > 0) ? Get_Component_Phase(FFT_InputBuf, fifth_harmonic_idx) : 0.0f;

    // ����г��FFTƵ��
    float third_harmonic_fft_freq = (third_harmonic_idx > 0) ? (float)third_harmonic_idx * sampling_frequency / (float)FFT_LENGTH : 0.0f;
    float fifth_harmonic_fft_freq = (fifth_harmonic_idx > 0) ? (float)fifth_harmonic_idx * sampling_frequency / (float)FFT_LENGTH : 0.0f;
    
    // ��г��FFTƵ��ӳ�������Ƶ��
    float third_harmonic_freq = (third_harmonic_idx > 0) ? Map_FFT_To_Input_Frequency(third_harmonic_fft_freq) : 0.0f;
    float fifth_harmonic_freq = (fifth_harmonic_idx > 0) ? Map_FFT_To_Input_Frequency(fifth_harmonic_fft_freq) : 0.0f;

    // �������г����Ϣ
    waveform_info->third_harmonic.frequency = third_harmonic_freq;
    waveform_info->third_harmonic.amplitude = third_harmonic_amp;
    waveform_info->third_harmonic.phase = third_harmonic_phase;
    waveform_info->third_harmonic.relative_amp = (fundamental_amp > 0.0f) ? third_harmonic_amp / fundamental_amp : 0.0f;

    // ������г����Ϣ
    waveform_info->fifth_harmonic.frequency = fifth_harmonic_freq;
    waveform_info->fifth_harmonic.amplitude = fifth_harmonic_amp;
    waveform_info->fifth_harmonic.phase = fifth_harmonic_phase;
    waveform_info->fifth_harmonic.relative_amp = (fundamental_amp > 0.0f) ? fifth_harmonic_amp / fundamental_amp : 0.0f;

    // ��������ڻ�������λ��
    if (third_harmonic_idx > 0)
    {
        float third_phase_diff = Calculate_Phase_Difference(third_harmonic_phase, fundamental_phase);
        waveform_info->third_harmonic.phase = third_phase_diff; // �洢��λ������Ǿ�����λ
    }

    if (fifth_harmonic_idx > 0)
    {
        float fifth_phase_diff = Calculate_Phase_Difference(fifth_harmonic_phase, fundamental_phase);
        waveform_info->fifth_harmonic.phase = fifth_phase_diff; // �洢��λ������Ǿ�����λ
    }

    // // ��ӡг���������
    // my_printf(&huart1, "����: Ƶ��=%.2f Hz, ����=%.4f, ��λ=%.2f rad\r\n",
    //           waveform_info->frequency, fundamental_amp, fundamental_phase);

    // my_printf(&huart1, "����г��: Ƶ��=%.2f Hz, ����=%.4f (%.2f%% of ����), ��λ��=%.2f rad\r\n",
    //           waveform_info->third_harmonic.frequency,
    //           waveform_info->third_harmonic.amplitude,
    //           waveform_info->third_harmonic.relative_amp * 100.0f,
    //           waveform_info->third_harmonic.phase);

    // my_printf(&huart1, "���г��: Ƶ��=%.2f Hz, ����=%.4f (%.2f%% of ����), ��λ��=%.2f rad\r\n",
    //           waveform_info->fifth_harmonic.frequency,
    //           waveform_info->fifth_harmonic.amplitude,
    //           waveform_info->fifth_harmonic.relative_amp * 100.0f,
    //           waveform_info->fifth_harmonic.phase);
}

/**
 * @brief ִ��FFT�任������Ƶ������
 * @param adc_val_buffer_f ADC����������
 * @param signal_frequency ָ��洢Ƶ�ʵı�����ָ��
 * @return ��������ö��ֵ
 */
ADC_WaveformType Analyze_Frequency_And_Type(uint32_t *adc_val_buffer_f, float *signal_frequency)
{
    float sampling_interval_us = dac_app_get_adc_sampling_interval_us();
    float sampling_frequency = 1000000.0f / sampling_interval_us;

    // ִ��FFT�任
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[2 * i] = (float)adc_val_buffer_f[i] / 4096.0f * 3.3f; // ʵ��
        FFT_InputBuf[2 * i + 1] = 0;                                       // �鲿
    }

    arm_cfft_radix4_f32(&scfft, FFT_InputBuf);                  // FFT����
    arm_cmplx_mag_f32(FFT_InputBuf, FFT_OutputBuf, FFT_LENGTH); // ȡģ�÷�ֵ

    // ֱ������
    float dc_component = FFT_OutputBuf[0];

    // 1. Ѱ�һ�Ƶ������ֵ��
    float fundamental_amp = 0.0f;
    int fundamental_idx = 0;

    for (int i = 1; i < FFT_LENGTH / 2; i++)
    {
        if (FFT_OutputBuf[i] > fundamental_amp)
        {
            fundamental_amp = FFT_OutputBuf[i];
            fundamental_idx = i;
        }
    }

    // ����FFTƵ��
    float fft_frequency = (float)fundamental_idx * sampling_frequency / (float)FFT_LENGTH;
    
    // ��FFTƵ��ӳ�������Ƶ��
    *signal_frequency = Map_FFT_To_Input_Frequency(fft_frequency);

    // ���ֱ���ź�
    if (dc_component > fundamental_amp * 5.0f)
    {
        *signal_frequency = 0.0f;
        return ADC_WAVEFORM_DC;
    }

    // �ź�ǿ�ȼ��
    if (fundamental_amp < 5.0f)
    {
        return ADC_WAVEFORM_UNKNOWN; // �ź�̫��
    }

    // 2. Ѱ������г������2*fundamental_idx��4*fundamental_idx֮�䣩
    float third_harmonic_amp = 0.0f;
    int third_harmonic_idx = 0;

    // ����������Χ
    int start_third = 2 * fundamental_idx;
    int end_third = 4 * fundamental_idx;

    // ����������Χ����ЧFFT������
    if (end_third > FFT_LENGTH / 2)
        end_third = FFT_LENGTH / 2;

    // ������г��Ԥ������Ѱ�ҷ�ֵ
    for (int i = start_third; i < end_third; i++)
    {
        if (FFT_OutputBuf[i] > third_harmonic_amp)
        {
            third_harmonic_amp = FFT_OutputBuf[i];
            third_harmonic_idx = i;
        }
    }

    // 3. Ѱ�����г������4*fundamental_idx��6*fundamental_idx֮�䣩
    float fifth_harmonic_amp = 0.0f;
    int fifth_harmonic_idx = 0;

    // ����������Χ
    int start_fifth = 4 * fundamental_idx;
    int end_fifth = 6 * fundamental_idx;

    // ����������Χ����ЧFFT������
    if (end_fifth > FFT_LENGTH / 2)
        end_fifth = FFT_LENGTH / 2;

    // �����г��Ԥ������Ѱ�ҷ�ֵ
    for (int i = start_fifth; i < end_fifth; i++)
    {
        if (FFT_OutputBuf[i] > fifth_harmonic_amp)
        {
            fifth_harmonic_amp = FFT_OutputBuf[i];
            fifth_harmonic_idx = i;
        }
    }

    // ����ҵ���г���Ƿ������壨��������Ϊ��Ƶ��5%��
    if (third_harmonic_amp < fundamental_amp * 0.05f)
    {
        third_harmonic_amp = 0.0f;
    }

    if (fifth_harmonic_amp < fundamental_amp * 0.05f)
    {
        fifth_harmonic_amp = 0.0f;
    }

    // ����г������
    float third_ratio = (third_harmonic_amp > 0) ? (third_harmonic_amp / fundamental_amp) : 0;
    float fifth_ratio = (fifth_harmonic_amp > 0) ? (fifth_harmonic_amp / fundamental_amp) : 0;

    //    �������
//    my_printf(&huart1, "��Ƶ����: %d, ����: %.3f\r\n", fundamental_idx, fundamental_amp);
//    my_printf(&huart1, "����г������: %d, ����: %.3f, ����: %.3f\r\n",
//              third_harmonic_idx, third_harmonic_amp, third_ratio);
//    my_printf(&huart1, "���г������: %d, ����: %.3f, ����: %.3f\r\n",
//              fifth_harmonic_idx, fifth_harmonic_amp, fifth_ratio);

    // 4. �����ж�

    // ���û������г���������Ҳ�
    if (third_ratio < 0.05f && fifth_ratio < 0.05f)
    {
        return ADC_WAVEFORM_SINE;
    }

    // ���۱���
    const float TRIANGLE_THIRD_END = 1.0f / 15.0f; // ���ǲ�����г�����۱�������(��Ϊ��һ����ȫ�غ�)
    const float SQUARE_THIRD_END = 1.0f / 5.0f;    // ��������г�����۱�������(��Ϊ��һ����ȫ�غ�)

    if (third_ratio > SQUARE_THIRD_END)
    {
        return ADC_WAVEFORM_SQUARE; // ����
    }
    else if (third_ratio > TRIANGLE_THIRD_END)
    {
        return ADC_WAVEFORM_TRIANGLE; // ���ǲ�
    }
    else
    {
        return ADC_WAVEFORM_UNKNOWN; // δ֪����
    }
}

/**
 * @brief ��ȡ��������
 * @param adc_val_buffer_f ADC����������
 * @return ��������ö��ֵ
 */
ADC_WaveformType Get_Waveform_Type(uint32_t *adc_val_buffer_f)
{
    float frequency;
    return Analyze_Frequency_And_Type(adc_val_buffer_f, &frequency);
}

const char *GetWaveformTypeString(ADC_WaveformType waveform)
{
    switch (waveform)
    {
    case ADC_WAVEFORM_DC:
        return "ֱ��";
    case ADC_WAVEFORM_SINE:
        return "���Ҳ�";
    case ADC_WAVEFORM_TRIANGLE:
        return "���ǲ�";
    case ADC_WAVEFORM_SQUARE:
        return "����";
    case ADC_WAVEFORM_UNKNOWN:
        return "δ֪����";
    default:
        return "��Ч����";
    }
}

/**
 * @brief ������������֮�����λ��
 * @param adc_val_buffer1 ��һ��ADC����������
 * @param adc_val_buffer2 �ڶ���ADC����������
 * @param frequency �ź�Ƶ��
 * @return ��λ������ƣ���ΧΪ-PI��PI��
 */
float Get_Phase_Difference(uint32_t *adc_val_buffer1, uint32_t *adc_val_buffer2, float frequency)
{
    // ���Ƶ����Ч������0
    if (frequency <= 0.0f)
    {
        return 0.0f;
    }

    // �����һ���źŵ���λ
    float phase1 = Get_Waveform_Phase(adc_val_buffer1, frequency);

    // ����ڶ����źŵ���λ
    float phase2 = Get_Waveform_Phase(adc_val_buffer2, frequency);

    // ������λ��
    float phase_diff = Calculate_Phase_Difference(phase1, phase2);

    // ��λ��ת��Ϊ��������ӡ��־
    float phase_diff_degrees = phase_diff * 180.0f / PI;

    my_printf(&huart1, "��λ1: %.2f ����, ��λ2: %.2f ����, ��ֵ: %.2f ���� (%.2f ��)\r\n",
              phase1, phase2, phase_diff, phase_diff_degrees);

    return phase_diff;
}

/**
 * @brief ��ȡ������Ϣ�������������͡�Ƶ�ʡ����ֵ����ֵ����Чֵ����λ��г����Ϣ
 * @param adc_val_buffer_f ADC����������
 * @return �����������͡�Ƶ�ʡ����ֵ����ֵ����Чֵ����λ��г����Ϣ�Ľṹ��
 */
WaveformInfo Get_Waveform_Info(uint32_t *adc_val_buffer_f)
{
    // ��ʼ���ṹ�壬����г����Ϣ
    WaveformInfo result = {
        .waveform_type = ADC_WAVEFORM_UNKNOWN,
        .frequency = 0.0f,
        .vpp = 0.0f,
        .mean = 0.0f,
        .rms = 0.0f,
        .phase = 0.0f,
        .third_harmonic = {0.0f, 0.0f, 0.0f, 0.0f},
        .fifth_harmonic = {0.0f, 0.0f, 0.0f, 0.0f}};

    // ������ֵ����ֵ����Чֵ
    result.vpp = Get_Waveform_Vpp(adc_val_buffer_f, &result.mean, &result.rms);

    // ��ȡ�������ͺ�Ƶ��
    result.waveform_type = Analyze_Frequency_And_Type(adc_val_buffer_f, &result.frequency);

    // ������λ��ֻ�е����β���DC�ź���Ƶ����Чʱ�ż�����λ��
    if (result.waveform_type != ADC_WAVEFORM_DC && result.frequency > 0.0f)
    {
        result.phase = Get_Waveform_Phase(adc_val_buffer_f, result.frequency);

        // ����г���ɷ֣��������Ⱥ���λ��
        Analyze_Harmonics(adc_val_buffer_f, &result);
    }

    // ��ӡ���λ�����Ϣ
    //    my_printf(&huart1, "��������: %s\r\n", GetWaveformTypeString(result.waveform_type));
    //    my_printf(&huart1, "Ƶ��: %.2f Hz, ���ֵ: %.2f V, ƽ��ֵ: %.2f V, ��Чֵ: %.2f V, ��λ: %.2f ����\r\n",
    //              result.frequency, result.vpp, result.mean, result.rms, result.phase);

    // // ��ӡг����Ϣ
    // if (result.waveform_type != ADC_WAVEFORM_DC && result.frequency > 0.0f)
    // {
    //     my_printf(&huart1, "����г��: Ƶ��=%.2f Hz, ���ȱ�=%.2f%%, ��λ��=%.2f ����\r\n",
    //               result.third_harmonic.frequency,
    //               result.third_harmonic.relative_amp * 100.0f,
    //               result.third_harmonic.phase);

    //     my_printf(&huart1, "���г��: Ƶ��=%.2f Hz, ���ȱ�=%.2f%%, ��λ��=%.2f ����\r\n",
    //               result.fifth_harmonic.frequency,
    //               result.fifth_harmonic.relative_amp * 100.0f,
    //               result.fifth_harmonic.phase);
    // }

    return result;
}
