/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"     /* ADC模块：用于模拟信号采集 */
#include "dac.h"     /* DAC模块：用于模拟信号输出 */
#include "dma.h"     /* DMA模块：用于直接内存访问，减轻CPU负担 */
#include "fatfs.h"   /* FATFS文件系统：用于SD卡文件操作 */
#include "i2c.h"     /* I2C通信：用于与OLED等I2C设备通信 */
#include "sdio.h"    /* SDIO接口：用于SD卡通信 */
#include "spi.h"     /* SPI通信：用于与Flash等SPI设备通信 */
#include "tim.h"     /* 定时器：用于精确定时功能 */
#include "usart.h"   /* 串口：用于与PC等设备通信 */
#include "gpio.h"    /* GPIO：通用输入输出口控制 */

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"   /* 标准输入输出：提供printf等函数 */
#include "mydefine.h" /* 自定义宏定义和函数：项目配置和功能集合 */
#include "string.h"  /* 字符串处理：提供字符串操作函数 */
#include "ff.h"      /* FatFs文件系统核心：提供文件操作API */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* 本节用于声明自定义类型，如结构体、枚举等 */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 本节用于项目内部的宏定义 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* 本节用于定义复杂的宏函数 */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* 本节用于声明私有函数原型 */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ===== 信号生成相关参数 ===== */
// 注意：峰值幅度是相对于中心电压 (Vref/2) 的。
//       如果 Vref = 3.3V, 中心电压 = 1.65V。
//       峰值幅度 1000mV 意味着输出电压范围大约在 0.65V 到 2.65V 之间。
uint32_t initial_frequency = 100;      // 初始频率：单位Hz，可通过函数修改
uint16_t initial_peak_amplitude = 1000; // 初始峰值幅度：单位mV，可通过函数修改

/* ===== 显示相关变量 ===== */
u8g2_t u8g2; // 全局u8g2实例：用于控制OLED显示，管理显示缓冲区和绘图操作
/* USER CODE END 0 */

/**
  * @brief  应用程序入口点
  * @retval int 未使用返回值
  * @note   此函数是程序的主入口点，初始化系统并执行主循环
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* 初始化前的预备工作可以放在这里 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init(); /* HAL库初始化：配置SysTick和中断优先级等 */

  /* USER CODE BEGIN Init */
  /* 其他自定义初始化可以放在这里 */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config(); /* 系统时钟配置：设置CPU和外设时钟 */

  /* USER CODE BEGIN SysInit */
  /* 系统级初始化可以放在这里 */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();     /* GPIO初始化：配置所有使用的IO口 */
  MX_DMA_Init();      /* DMA初始化：配置用于ADC、UART等的DMA通道 */
  MX_USART1_UART_Init(); /* UART1初始化：配置用于调试输出的串口 */
  MX_ADC1_Init();     /* ADC1初始化：配置用于信号采集的ADC通道 */
  MX_DAC_Init();      /* DAC初始化：配置用于信号生成的DAC通道 */
  MX_TIM3_Init();     /* 定时器3初始化：可用于PWM输出或定时触发 */
  MX_TIM6_Init();     /* 定时器6初始化：通常用作基本定时器 */
  MX_TIM14_Init();    /* 定时器14初始化：可用于其他定时功能 */
  MX_I2C1_Init();     /* I2C1初始化：用于与OLED通信 */
  MX_SPI2_Init();     /* SPI2初始化：用于与Flash存储器通信 */
  MX_SDIO_SD_Init();  /* SDIO初始化：配置SD卡接口 */
  MX_FATFS_Init();    /* FatFs初始化：准备文件系统 */
  /* USER CODE BEGIN 2 */
  /* ===== 应用层初始化开始 ===== */
  
  /* FFT模块初始化：用于频谱分析 */
  My_FFT_Init();
  
  /* 串口环形缓冲区初始化：用于异步处理串口数据 */
  rt_ringbuffer_init(&uart_ringbuffer, ringbuffer_pool, sizeof(ringbuffer_pool));
  
  /* 信号生成应用初始化（注释掉的部分） */
  //  dac_app_init(initial_frequency, initial_peak_amplitude);
  //	dac_app_set_frequency(1000);
  //	dac_app_set_waveform(WAVEFORM_TRIANGLE);
  
  /* 按钮应用初始化：处理按键输入 */
  app_btn_init();
  
  /* OLED显示初始化：准备显示硬件 */
  OLED_Init();
  
  /* ADC采样定时器DMA初始化：用于连续信号采集 */
  adc_tim_dma_init();
  
  /* ===== U8G2图形库初始化 ===== */
  // 1. Setup: 这是最关键的一步，它配置了 u8g2 实例。
  //    - 选择与硬件匹配的 setup 函数 (SSD1306, I2C, 128x32, Full Buffer)。
  //    - &u8g2: 指向要配置的 u8g2 结构体实例的指针。
  //    - U8G2_R0: 旋转设置。U8G2_R0=0°, U8G2_R1=90°, U8G2_R2=180°, U8G2_R3=270°。
  //    - u8x8_byte_hw_i2c: 指向你的硬件 I2C 字节传输回调函数的指针。
  //    - u8g2_gpio_and_delay_stm32: 指向你的 GPIO 和延时回调函数的指针。
  u8g2_Setup_ssd1306_i2c_128x32_univision_f(&u8g2, U8G2_R0, u8x8_byte_hw_i2c, u8g2_gpio_and_delay_stm32);
  //    - &u8g2: u8g2 结构体指针
  //    - U8G2_R0: 旋转设置 (0度)
  // 2. Init Display: 发送初始化序列到 OLED
  u8g2_InitDisplay(&u8g2);
  // 3. Set Power Save: 唤醒屏幕。
  //    - 参数 0 表示关闭省电模式 (屏幕亮起)。
  //    - 参数 1 表示进入省电模式 (屏幕熄灭)。
  u8g2_SetPowerSave(&u8g2, 0);


  /* ===== WouoUI用户界面框架初始化 ===== */
  // 1. 选择默认UI (如果 WouoUI_user.c 中定义了多个 UI 实例)
  WouoUI_SelectDefaultUI(); 

  // 2. 绑定缓存刷新函数 (关键步骤)
  //    将你实现的 OLED_SendBuff 函数地址传递给 WouoUI 框架
  WouoUI_AttachSendBuffFun(OLED_SendBuff); 

  // 3. 初始化用户菜单 (执行 WouoUI_user.c/h 中定义的菜单结构初始化)
  PIDMenu_Init(); // 函数名取决于用户文件，通常是初始化页面、列表项等
  /* ===== WouoUI初始化完成 ===== */
  
  /* SPI Flash测试（注释掉的部分） */
  //	test_spi_flash();
  
  /* 输出系统初始化完成信息 */
  my_printf(&huart1, "\r\n--- System Init OK ---\r\n");
  
  /* ===== LittleFS文件系统初始化 ===== */
  /* 初始化SPI Flash存储设备 */
  spi_flash_init(); // 确保SPI Flash已就绪
  my_printf(&huart1, "LFS: Initializing storage backend...\r\n");
  /* 初始化LittleFS存储后端 */
  if (lfs_storage_init(&cfg) != LFS_ERR_OK)
  {
    my_printf(&huart1, "LFS: Storage backend init FAILED! Halting.\r\n");
    while (1)
      ; // 存储后端初始化失败时死循环
  }
  my_printf(&huart1, "LFS: Storage backend init OK.\r\n");

  /* 运行LittleFS基本测试：挂载、格式化(如需)和读写测试 */
  lfs_basic_test(); 
  
  /* ===== Shell命令行界面初始化 ===== */
  // 初始化Shell并传入文件系统实例
  shell_init(&lfs);
    
  // 设置UART句柄
  shell_set_uart(&huart1);
  
  /* 初始化任务调度器：管理多任务执行 */
  scheduler_init();
  
  /* 测试SD卡和FAT文件系统功能 */
  test_sd_fatfs();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1) /* 主循环：程序将一直在此循环中运行 */
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 运行任务调度器：处理所有注册的任务 */
    scheduler_run();
  }
  /* USER CODE END 3 */
}

/**
  * @brief 系统时钟配置
  * @retval None
  * @note  配置系统时钟、PLL、分频器等
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE(); /* 使能电源控制时钟 */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3); /* 设置电压调节器输出级别 */

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE; /* 使用外部高速时钟(HSE) */
  RCC_OscInitStruct.HSEState = RCC_HSE_ON; /* 开启HSE */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON; /* 启用PLL */
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE; /* PLL时钟源为HSE */
  RCC_OscInitStruct.PLL.PLLM = 15; /* HSE分频系数M=15 */
  RCC_OscInitStruct.PLL.PLLN = 144; /* PLL倍频系数N=144 */
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; /* PLL输出分频系数P=2 */
  RCC_OscInitStruct.PLL.PLLQ = 5; /* PLL输出分频系数Q=5，用于USB、SDIO等 */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; /* 系统时钟源为PLL */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; /* AHB时钟 = 系统时钟 */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4; /* APB1时钟 = AHB时钟/4 */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2; /* APB2时钟 = AHB时钟/2 */

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* 用户代码段4：可添加自定义函数实现 */
/* USER CODE END 4 */

/**
  * @brief  错误处理函数
  * @retval None
  * @note   在发生错误时被调用，通常会导致系统挂起
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 用户可以添加自己的实现来报告HAL错误返回状态 */
  __disable_irq(); /* 禁用中断 */
  while (1) /* 错误处理死循环 */
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  报告断言错误的源文件名和源行号
  * @param  file: 指向源文件名的指针
  * @param  line: 断言错误行源行号
  * @retval None
  * @note   在使用assert_param宏检测到参数错误时被调用
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* 用户可以添加自己的实现来报告文件名和行号，
     例如: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
