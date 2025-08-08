#include "btn_app.h"
#include "ebtn.h"
#include "gpio.h"

extern uint8_t ucLed[6];
extern WaveformInfo wave_data;
// 频率调节步进值
#define FREQ_STEP 100 // 每次增加/减少100Hz

// 波形幅度调节步进值
#define AMP_STEP 100 // 每次增加/减少100mV

// 当前波形类型和最小最大频率限制
static dac_waveform_t current_wave_type = WAVEFORM_SINE;
#define MIN_FREQUENCY 100   // 最小频率100Hz
#define MAX_FREQUENCY 50000 // 最大频率10kHz

// 波形幅度限制
#define MIN_AMPLITUDE 100  // 最小峰值幅度100mV
#define MAX_AMPLITUDE 1650 // 最大峰值幅度1.65V (3.3V的一半)

/*
    最大值（MAX_AMPLITUDE = 1650mV）：
    DAC参考电压(DAC_VREF_MV)为3300mV (3.3V)
    波形输出是围绕中心电压(VREF/2)上下变化的
    最大峰值幅度就是中心电压值：3300mV/2 = 1650mV
    这样可确保波形不会超出DAC的输出范围（0-3.3V）
    完整波形的峰峰值最大为3.3V


    最小值（MIN_AMPLITUDE = 100mV）：
    设为100mV是为保证波形有一定的可见幅度
    过小的幅度在实际应用中难以观察或使用
    这是基于实用性考虑的经验值
*/

uint32_t new_freq;
uint32_t current_freq;
uint16_t current_amp;
uint16_t new_amp;
uint8_t uart_send_flag = 0;

typedef enum
{
    USER_BUTTON_0 = 0,
    USER_BUTTON_1,
    USER_BUTTON_2,
    USER_BUTTON_3,
    USER_BUTTON_4,
    USER_BUTTON_5,
    USER_BUTTON_MAX,

    //    USER_BUTTON_COMBO_0 = 0x100,
    //    USER_BUTTON_COMBO_1,
    //    USER_BUTTON_COMBO_2,
    //    USER_BUTTON_COMBO_3,
    //    USER_BUTTON_COMBO_MAX,
} user_button_t;

/*  Debounce time in milliseconds, Debounce time in milliseconds for release event, Minimum pressed time for valid click event, Maximum ...,
    Maximum time between 2 clicks to be considered consecutive click, Time in ms for periodic keep alive event, Max number of consecutive clicks */
static const ebtn_btn_param_t defaul_ebtn_param = EBTN_PARAMS_INIT(20, 0, 20, 1000, 0, 1000, 10);

static ebtn_btn_t btns[] = {
    EBTN_BUTTON_INIT(USER_BUTTON_0, &defaul_ebtn_param),
    EBTN_BUTTON_INIT(USER_BUTTON_1, &defaul_ebtn_param),
    EBTN_BUTTON_INIT(USER_BUTTON_2, &defaul_ebtn_param),
    EBTN_BUTTON_INIT(USER_BUTTON_3, &defaul_ebtn_param),
    EBTN_BUTTON_INIT(USER_BUTTON_4, &defaul_ebtn_param),
    EBTN_BUTTON_INIT(USER_BUTTON_5, &defaul_ebtn_param),
};

// static ebtn_btn_combo_t btns_combo[] = {
//     EBTN_BUTTON_COMBO_INIT_RAW(USER_BUTTON_COMBO_0, &defaul_ebtn_param, EBTN_EVT_MASK_ONCLICK),
//     EBTN_BUTTON_COMBO_INIT_RAW(USER_BUTTON_COMBO_1, &defaul_ebtn_param, EBTN_EVT_MASK_ONCLICK),
// };

uint8_t prv_btn_get_state(struct ebtn_btn *btn)
{
    switch (btn->key_id)
    {
    case USER_BUTTON_0:
        return !HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15);
    case USER_BUTTON_1:
        return !HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_13);
    case USER_BUTTON_2:
        return !HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_11);
    case USER_BUTTON_3:
        return !HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_9);
    case USER_BUTTON_4:
        return !HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_7);
    case USER_BUTTON_5:
        return !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
    default:
        return 0;
    }
}

// void prv_btn_event(struct ebtn_btn *btn, ebtn_evt_t evt)
//{
//     if ((btn->key_id == USER_BUTTON_0) && (ebtn_click_get_count(btn) == 1))
//     {
//         ucLed[0] ^= 1;
//         uart_send_flag ^= 1;
//         wave_analysis_flag = 1;
//     }

//    if ((btn->key_id == USER_BUTTON_1) && (ebtn_click_get_count(btn) == 1))
//    {
//        // 按键1切换波形
//        ucLed[1] ^= 1;

//        // 切换波形类型：正弦波->方波->三角波->正弦波...
//        switch (current_wave_type)
//        {
//        case WAVEFORM_SINE:
//            current_wave_type = WAVEFORM_SQUARE;
//            break;
//        case WAVEFORM_SQUARE:
//            current_wave_type = WAVEFORM_TRIANGLE;
//            break;
//        case WAVEFORM_TRIANGLE:
//            current_wave_type = WAVEFORM_SINE;
//            break;
//        default:
//            current_wave_type = WAVEFORM_SINE;
//            break;
//        }

//        // 设置新的波形类型
//        dac_app_set_waveform(current_wave_type);
//    }

//    if ((btn->key_id == USER_BUTTON_2) && (ebtn_click_get_count(btn) == 1))
//    {
//        // 按键2增加频率
//        ucLed[2] ^= 1;

//        // 获取当前频率，然后增加FREQ_STEP
//        current_freq = dac_app_get_update_frequency() / WAVEFORM_SAMPLES;
//        new_freq = current_freq + FREQ_STEP;

//        // 限制最大频率
//        if (new_freq > MAX_FREQUENCY)
//            new_freq = MAX_FREQUENCY;

//        // 设置新频率
//        dac_app_set_frequency(new_freq);
//    }

//    if ((btn->key_id == USER_BUTTON_3) && (ebtn_click_get_count(btn) == 1))
//    {
//        // 按键3减少频率
//        ucLed[3] ^= 1;

//        // 获取当前频率，然后减少FREQ_STEP
//        current_freq = dac_app_get_update_frequency() / WAVEFORM_SAMPLES;
//        new_freq = (current_freq > FREQ_STEP) ? (current_freq - FREQ_STEP) : MIN_FREQUENCY;

//        // 限制最小频率
//        if (new_freq < MIN_FREQUENCY)
//            new_freq = MIN_FREQUENCY;

//        // 设置新频率
//        dac_app_set_frequency(new_freq);
//    }

//    if ((btn->key_id == USER_BUTTON_4) && (ebtn_click_get_count(btn) == 1))
//    {
//        // 按键4增加峰值幅度
//        ucLed[4] ^= 1;

//        // 获取当前峰值幅度并增加AMP_STEP
//        current_amp = dac_app_get_amplitude();
//        new_amp = current_amp + AMP_STEP;

//        // 限制最大幅度
//        if (new_amp > MAX_AMPLITUDE)
//            new_amp = MAX_AMPLITUDE;

//        // 设置新幅度
//        dac_app_set_amplitude(new_amp);
//    }

//    if ((btn->key_id == USER_BUTTON_5) && (ebtn_click_get_count(btn) == 1))
//    {
//        // 按键5减少峰值幅度
//        ucLed[5] ^= 1;

//        // 获取当前峰值幅度并减少AMP_STEP
//        current_amp = dac_app_get_amplitude();
//        new_amp = (current_amp > AMP_STEP) ? (current_amp - AMP_STEP) : MIN_AMPLITUDE;

//        // 限制最小幅度
//        if (new_amp < MIN_AMPLITUDE)
//            new_amp = MIN_AMPLITUDE;

//        // 设置新幅度
//        dac_app_set_amplitude(new_amp);
//    }
//}

void prv_btn_event(struct ebtn_btn *btn, ebtn_evt_t evt)
{
    if (evt == EBTN_EVT_KEEPALIVE) // 长按事件
    {
        switch (btn->key_id)
        {
        case USER_BUTTON_0:
            WOUOUI_MSG_QUE_SEND(msg_up);
            break;
        case USER_BUTTON_1:
            WOUOUI_MSG_QUE_SEND(msg_down);
            break;
        case USER_BUTTON_2:
            WOUOUI_MSG_QUE_SEND(msg_left);
            break;
        case USER_BUTTON_3:
            WOUOUI_MSG_QUE_SEND(msg_right);
            break;
        case USER_BUTTON_4:
            WOUOUI_MSG_QUE_SEND(msg_return);
            break;
        case USER_BUTTON_5:
            WOUOUI_MSG_QUE_SEND(msg_click);
            break;
        default:
            break;
        }
    }
    else
    {
        if ((btn->key_id == USER_BUTTON_0) && (ebtn_click_get_count(btn) == 1))
        {
            WOUOUI_MSG_QUE_SEND(msg_up);
        }

        if ((btn->key_id == USER_BUTTON_1) && (ebtn_click_get_count(btn) == 1))
        {
            WOUOUI_MSG_QUE_SEND(msg_down);
        }

        if ((btn->key_id == USER_BUTTON_2) && (ebtn_click_get_count(btn) == 1))
        {
            WOUOUI_MSG_QUE_SEND(msg_left);
        }

        if ((btn->key_id == USER_BUTTON_3) && (ebtn_click_get_count(btn) == 1))
        {
            WOUOUI_MSG_QUE_SEND(msg_right);
        }

        if ((btn->key_id == USER_BUTTON_4) && (ebtn_click_get_count(btn) == 1))
        {
            WOUOUI_MSG_QUE_SEND(msg_return); // 返回
        }

        if ((btn->key_id == USER_BUTTON_5) && (ebtn_click_get_count(btn) == 1))
        {
            WOUOUI_MSG_QUE_SEND(msg_click); // 确定
        }
    }

    //    if(btn->key_id == USER_BUTTON_COMBO_0) {
    //        rlog_str("btn combo click evt");
    //    }
}

void app_btn_init(void)
{
    // ebtn_init(btns, EBTN_ARRAY_SIZE(btns), btns_combo, EBTN_ARRAY_SIZE(btns_combo), prv_btn_get_state, prv_btn_event);
    ebtn_init(btns, EBTN_ARRAY_SIZE(btns), NULL, 0, prv_btn_get_state, prv_btn_event);

    //    ebtn_combo_btn_add_btn(&btns_combo[0], USER_BUTTON_0);
    //    ebtn_combo_btn_add_btn(&btns_combo[0], USER_BUTTON_1);

    //    ebtn_combo_btn_add_btn(&btns_combo[1], USER_BUTTON_2);
    //    ebtn_combo_btn_add_btn(&btns_combo[1], USER_BUTTON_3);

    HAL_TIM_Base_Start_IT(&htim14);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == (&htim14))
    {
        ebtn_process(uwTick);
    }
}

void btn_task(void)
{
	
}
