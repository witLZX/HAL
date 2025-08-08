#include "led_app.h"
#include "gpio.h"
#include "math.h"
uint8_t ucLed[6] = {1,0,1,0,1,0};  // LED ״̬����

/**
 * @brief ��ʾ��ر�Led
 *
 *
 * @param ucLed Led���ݴ�������
 */
void led_disp(uint8_t *ucLed)
{
    // ���ڼ�¼��ǰ LED ״̬����ʱ����
    uint8_t temp = 0x00;
    // ��¼֮ǰ LED ״̬�ı����������ж��Ƿ���Ҫ������ʾ
    static uint8_t temp_old = 0xff;

    for (int i = 0; i < 6; i++)
    {
        // ��LED״̬���ϵ�temp�����У���������Ƚ�
        if (ucLed[i]) temp |= (1<<i); // ����iλ��1
    }

    // ������ǰ״̬��֮ǰ״̬��ͬ��ʱ�򣬲Ÿ�����ʾ
    if (temp_old != temp)
    {
        // ����GPIO��ʼ����������ö�Ӧ����
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, (temp & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 0
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, (temp & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 1
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, (temp & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 2
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, (temp & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 3
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12,  (temp & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 4
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13,  (temp & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 5
        
        // ���¾�״̬
        temp_old = temp;
    }
}

/**
 * @brief LED ��ʾ������
 *
 * ÿ�ε��øú���ʱ��LED �Ƹ��� ucLed �����е�ֵ�������ǿ������ǹر�
 */
void led_task(void)
{
    // ��������ر���
//    static uint32_t breathCounter = 0;      // �����Ƽ�����
//    static uint8_t pwmCounter = 0;          // PWM������
//    static uint8_t brightness = 0;          // ��ǰ����ֵ
//    static const uint16_t breathPeriod = 2000; // ��������(ms)
//    static const uint8_t pwmMax = 10;       // PWM�������ֵ��10ms��
//    
//    // ���º���������
//    breathCounter = (breathCounter + 1) % breathPeriod;
//    
//    // �������Һ�����������ֵ(0-pwmMax)
//    brightness = (uint8_t)((sinf((2.0f * 3.14159f * breathCounter) / breathPeriod) + 1.0f) * pwmMax / 2.0f);
//    
//    // ����PWM������ (0-pwmMax)
//    pwmCounter = (pwmCounter + 1) % pwmMax;
//    
//    // ��������ֵ��PWM������ȷ��LED״̬
//    // ������Ϊ0ʱ��ȫϨ������ΪpwmMaxʱ����
//    ucLed[0] = (pwmCounter < brightness) ? 1 : 0;

    // ��������LED״̬
    led_disp(ucLed);
}

/*

void led_proc(void)
{
    // ��������ر���
    static uint32_t breathCounter = 0;      // �����Ƽ�����
    static uint8_t pwmCounter = 0;          // PWM������
    static const uint16_t breathPeriod = 4000; // ��С��������(ms)
    static const uint8_t pwmMax = 10;       // PWM�������ֵ
    static const float phaseShift = 3.14159f / 3.0f; // ������λ���/3����Լ2LEDͬʱ����
    
    // ���º���������
    breathCounter = (breathCounter + 1) % breathPeriod;
    
    // ����PWM������ (0-pwmMax)
    pwmCounter = (pwmCounter + 1) % pwmMax;
    
    // Ϊÿ��LED�������Ȳ�����״̬
    for(uint8_t i = 0; i < 6; i++) 
    {
        // ����λ��ĽǶȼ��㣬ʵ��˳�����Ч��
        float angle = (2.0f * 3.14159f * breathCounter) / breathPeriod - i * phaseShift;
        // ʹ���ݺ�����ǿ�ԱȶȲ�����ȫ��ʱ��
        float sinValue = sinf(angle);
        // ������ֵ���и�ǿ��ָ����������ȫ��ʱ��
        float enhancedValue = sinValue > 0 ? powf(sinValue, 0.5f) : -powf(-sinValue, 0.5f);
        // ǿ��ѹ���������ߣ�ֻ�з�ֵ�����Žӽ�ȫ��
        enhancedValue = enhancedValue > 0.7f ? enhancedValue : enhancedValue * 0.6f;
        // ��������ֵ(0-pwmMax)
        uint8_t brightness = (uint8_t)((enhancedValue + 1.0f) * pwmMax / 2.0f);
        // ����LED״̬
        ucLed[i] = (pwmCounter < brightness) ? 1 : 0;
    }

    // ����LED��ʾ
    led_disp(ucLed);
}

*/


