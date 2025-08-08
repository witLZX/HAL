#include "led_app.h"
#include "gpio.h"
#include "math.h"
uint8_t ucLed[6] = {1,0,1,0,1,0};  // LED 状态数组

/**
 * @brief 显示或关闭Led
 *
 *
 * @param ucLed Led数据储存数组
 */
void led_disp(uint8_t *ucLed)
{
    // 用于记录当前 LED 状态的临时变量
    uint8_t temp = 0x00;
    // 记录之前 LED 状态的变量，用于判断是否需要更新显示
    static uint8_t temp_old = 0xff;

    for (int i = 0; i < 6; i++)
    {
        // 将LED状态整合到temp变量中，方便后续比较
        if (ucLed[i]) temp |= (1<<i); // 将第i位置1
    }

    // 仅当当前状态与之前状态不同的时候，才更新显示
    if (temp_old != temp)
    {
        // 根据GPIO初始化情况，设置对应引脚
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, (temp & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 0
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, (temp & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 1
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, (temp & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 2
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, (temp & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 3
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12,  (temp & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 4
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13,  (temp & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 5
        
        // 更新旧状态
        temp_old = temp;
    }
}

/**
 * @brief LED 显示处理函数
 *
 * 每次调用该函数时，LED 灯根据 ucLed 数组中的值来决定是开启还是关闭
 */
void led_task(void)
{
    // 呼吸灯相关变量
//    static uint32_t breathCounter = 0;      // 呼吸灯计数器
//    static uint8_t pwmCounter = 0;          // PWM计数器
//    static uint8_t brightness = 0;          // 当前亮度值
//    static const uint16_t breathPeriod = 2000; // 呼吸周期(ms)
//    static const uint8_t pwmMax = 10;       // PWM周期最大值（10ms）
//    
//    // 更新呼吸计数器
//    breathCounter = (breathCounter + 1) % breathPeriod;
//    
//    // 根据正弦函数计算亮度值(0-pwmMax)
//    brightness = (uint8_t)((sinf((2.0f * 3.14159f * breathCounter) / breathPeriod) + 1.0f) * pwmMax / 2.0f);
//    
//    // 更新PWM计数器 (0-pwmMax)
//    pwmCounter = (pwmCounter + 1) % pwmMax;
//    
//    // 根据亮度值和PWM计数器确定LED状态
//    // 当亮度为0时完全熄灭，亮度为pwmMax时常亮
//    ucLed[0] = (pwmCounter < brightness) ? 1 : 0;

    // 更新其他LED状态
    led_disp(ucLed);
}

/*

void led_proc(void)
{
    // 呼吸灯相关变量
    static uint32_t breathCounter = 0;      // 呼吸灯计数器
    static uint8_t pwmCounter = 0;          // PWM计数器
    static const uint16_t breathPeriod = 4000; // 减小呼吸周期(ms)
    static const uint8_t pwmMax = 10;       // PWM周期最大值
    static const float phaseShift = 3.14159f / 3.0f; // 增大相位差（π/3），约2LED同时呼吸
    
    // 更新呼吸计数器
    breathCounter = (breathCounter + 1) % breathPeriod;
    
    // 更新PWM计数器 (0-pwmMax)
    pwmCounter = (pwmCounter + 1) % pwmMax;
    
    // 为每个LED计算亮度并设置状态
    for(uint8_t i = 0; i < 6; i++) 
    {
        // 带相位差的角度计算，实现顺序呼吸效果
        float angle = (2.0f * 3.14159f * breathCounter) / breathPeriod - i * phaseShift;
        // 使用幂函数增强对比度并缩短全亮时间
        float sinValue = sinf(angle);
        // 对正弦值进行更强的指数处理，减少全亮时间
        float enhancedValue = sinValue > 0 ? powf(sinValue, 0.5f) : -powf(-sinValue, 0.5f);
        // 强制压缩亮度曲线，只有峰值附近才接近全亮
        enhancedValue = enhancedValue > 0.7f ? enhancedValue : enhancedValue * 0.6f;
        // 计算亮度值(0-pwmMax)
        uint8_t brightness = (uint8_t)((enhancedValue + 1.0f) * pwmMax / 2.0f);
        // 设置LED状态
        ucLed[i] = (pwmCounter < brightness) ? 1 : 0;
    }

    // 更新LED显示
    led_disp(ucLed);
}

*/


