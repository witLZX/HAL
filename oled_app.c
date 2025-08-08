#include "oled_app.h"

// PID 参数范围和步进宏定义
#define PID_PARAM_P_MIN 0    // P 参数最小值 (0.00)
#define PID_PARAM_P_MAX 1000 // P 参数最大值 (10.00)
#define PID_PARAM_P_STEP 10  // P 参数步进值 (0.10)

#define PID_PARAM_I_MIN 0    // I 参数最小值 (0.00)
#define PID_PARAM_I_MAX 1000 // I 参数最大值 (10.00)
#define PID_PARAM_I_STEP 10  // I 参数步进值 (0.10)

#define PID_PARAM_D_MIN 0   // D 参数最小值 (0.00)
#define PID_PARAM_D_MAX 100 // D 参数最大值 (1.00)
#define PID_PARAM_D_STEP 1  // D 参数步进值 (0.01)

/**
 * @brief	使用类似printf的方式显示字符串，显示6x8大小的ASCII字符
 * @param x  Character position on the X-axis  range：0 - 127
 * @param y  Character position on the Y-axis  range：0 - 3
 * 例如：oled_printf(0, 0, "Data = %d", dat);
 **/
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
  char buffer[512]; // 临时存储格式化后的字符串
  va_list arg;      // 处理可变参数
  int len;          // 最终字符串长度

  va_start(arg, format);
  // 安全地格式化字符串到 buffer
  len = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  OLED_ShowStr(x, y, buffer, 8);
  return len;
}

// u8g2 的 GPIO 和延时回调函数
uint8_t u8g2_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch (msg)
  {
  case U8X8_MSG_GPIO_AND_DELAY_INIT:
    // 初始化 GPIO (如果需要，例如 SPI 的 CS, DC, RST 引脚)
    // 对于硬件 I2C，这里通常不需要做什么
    break;
  case U8X8_MSG_DELAY_MILLI:
    // 原因: u8g2 内部某些操作需要毫秒级的延时等待。
    // 提供毫秒级延时，直接调用 HAL 库函数。
    HAL_Delay(arg_int);
    break;
  case U8X8_MSG_DELAY_10MICRO:
    // 实现10微秒延时，使用精确校准的空循环
    {
      // GD32系列通常运行速度为120-200MHz，每个循环大约需要3-4个时钟周期
      // 按160MHz计算，10μs需要约400-500个循环
      for (volatile uint32_t i = 0; i < 480; i++)
      {
        __NOP(); // 编译器不会优化掉这个指令
      }
    }
    break;
  case U8X8_MSG_DELAY_100NANO:
    // 实现100纳秒延时，使用多个NOP指令
    // 每个NOP指令大约需要1个时钟周期(约6ns@160MHz)
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    break;
  case U8X8_MSG_GPIO_I2C_CLOCK: // [[fallthrough]] // Fallthrough 注释表示有意为之
  case U8X8_MSG_GPIO_I2C_DATA:
    // 控制 SCL/SDA 引脚电平。这些仅在**软件模拟 I2C** 时需要实现。
    // 使用硬件 I2C 时，这些消息可以忽略，由 HAL 库处理。
    break;
  // --- 以下是 GPIO 相关的消息，主要用于按键输入或 SPI 控制 ---
  // 如果你的 u8g2 应用需要读取按键或控制 SPI 引脚 (CS, DC, Reset)，
  // 你需要在这里根据 msg 类型读取/设置对应的 GPIO 引脚状态。
  // 对于仅使用硬件 I2C 显示的场景，可以像下面这样简单返回不支持。
  case U8X8_MSG_GPIO_CS:
    // SPI 片选控制
    break;
  case U8X8_MSG_GPIO_DC:
    // SPI 数据/命令线控制
    break;
  case U8X8_MSG_GPIO_RESET:
    // 显示屏复位引脚控制
    break;
  case U8X8_MSG_GPIO_MENU_SELECT:
    u8x8_SetGPIOResult(u8x8, /* 读取选择键 GPIO 状态 */ 0);
    break;
  default:
    u8x8_SetGPIOResult(u8x8, 1); // 不支持的消息
    break;
  }
  return 1;
}

// u8g2 的硬件 I2C 通信回调函数
uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  static uint8_t buffer[32]; // u8g2 每次传输最大 32 字节
  static uint8_t buf_idx;
  uint8_t *data;

  switch (msg)
  {
  case U8X8_MSG_BYTE_SEND:
    // 原因: u8g2 通常不会一次性发送大量数据，而是分块发送。
    // 这个消息用于将一小块数据 (arg_int 字节) 从 u8g2 内部传递到我们的回调函数。
    // 我们需要将这些数据暂存到本地 buffer 中，等待 START/END_TRANSFER 信号。
    data = (uint8_t *)arg_ptr;
    while (arg_int > 0)
    {
      buffer[buf_idx++] = *data;
      data++;
      arg_int--;
    }
    break;
  case U8X8_MSG_BYTE_INIT:
    // 原因: 提供一个机会进行 I2C 外设的初始化。
    // 初始化 I2C (通常在 main 函数中已完成)
    // 由于我们在 main 函数中已经调用了 MX_I2C1_Init()，这里通常可以留空。
    break;
  case U8X8_MSG_BYTE_SET_DC:
    // 原因: 这个消息用于 SPI 通信中控制 Data/Command 选择引脚。
    // 设置数据/命令线 (I2C 不需要)
    // I2C 通过特定的控制字节 (0x00 或 0x40) 区分命令和数据，因此该消息对于 I2C 无意义。
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    // 原因: 标记一个 I2C 传输序列的开始。
    buf_idx = 0;
    // 我们在这里重置本地缓冲区的索引，准备接收新的数据块。
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    // 原因: 标记一个 I2C 传输序列的结束。
    // 此时，本地 buffer 中已经暂存了所有需要发送的数据块。
    // 这是执行实际 I2C 发送操作的最佳时机。
    // 发送缓冲区中的数据
    // 注意: u8x8_GetI2CAddress(u8x8) 返回的是 7 位地址 * 2 = 8 位地址
    if (HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buffer, buf_idx, 100) != HAL_OK)
    {
      return 0; // 发送失败
    }
    break;
  default:
    return 0;
  }
  return 1;
}

/* 缓存刷新函数 */
void OLED_SendBuff(uint8_t buff[4][128])
{
  // 获取 u8g2 的缓冲区指针
  uint8_t *u8g2_buffer = u8g2_GetBufferPtr(&u8g2);

  // 将数据拷贝到 u8g2 的缓冲区
  memcpy(u8g2_buffer, buff, 4 * 128);

  // 发送整个缓冲区到 OLED
  u8g2_SendBuffer(&u8g2);
}

// 页面对象
TitlePage main_menu;         // 第一级菜单（左右轮选择）
ListPage left_wheel_menu;    // 左轮的PID参数选择菜单
ListPage right_wheel_menu;   // 右轮的PID参数选择菜单
ValWin param_adjust_val_win; // 使用 ValWin 进行参数调整

// 左右轮PID参数数据
typedef struct
{
  float p;
  float i;
  float d;
} PIDParams;

PIDParams left_wheel_pid = {1.0, 0.1, 0.01};
PIDParams right_wheel_pid = {1.0, 0.1, 0.01};

// 第一级菜单选项（左右轮选择）
#define MAIN_MENU_NUM 2
Option main_menu_options[MAIN_MENU_NUM] = {
    {.text = (char *)"+ Left Wheel", .content = (char *)"Left"},
    {.text = (char *)"+ Right Wheel", .content = (char *)"Right"}};

// 第一级菜单图标
Icon main_menu_icons[MAIN_MENU_NUM] = {
    [0] = {0xFC, 0xFE, 0xFF, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
           0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x07, 0x07, 0x0F, 0x1F, 0x3F, 0xFF, 0xFE, 0xFC, 0xFF, 0x01,
           0x00, 0x00, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xFC, 0xFC,
           0x00, 0x00, 0xFC, 0xFC, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xF0, 0xC0, 0x00,
           0x00, 0x00, 0x03, 0x07, 0x0F, 0x1F, 0x3E, 0x3C, 0x3C, 0x3C, 0x1E, 0x1F, 0x0F, 0x03, 0x00, 0x00,
           0x1F, 0x3F, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0xC0, 0xF0, 0xFF, 0xCF, 0xDF, 0xFF, 0xFF, 0xFE, 0xFC,
           0xF8, 0xF8, 0xF0, 0xF0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0xF0,
           0xF8, 0xF8, 0xFC, 0xFE, 0xFF, 0xFF, 0xDF, 0xCF}, // logo
    [1] = {0xFC, 0xFE, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x83, 0x81, 0x01, 0x01, 0x81, 0xE1, 0xE1, 0xE1,
           0xE1, 0x81, 0x01, 0x81, 0x81, 0x83, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFE, 0xFC, 0xFF, 0x01,
           0x00, 0x00, 0x00, 0xE0, 0xE0, 0xF3, 0xFF, 0xFF, 0x3F, 0x0F, 0x07, 0x07, 0x03, 0x03, 0x07, 0x07,
           0x0F, 0x3F, 0xFF, 0xFF, 0xF7, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xE0, 0x80, 0x00,
           0x00, 0x01, 0x01, 0x3B, 0x7F, 0x7F, 0x7F, 0x3C, 0x78, 0xF8, 0xF0, 0xF0, 0xF8, 0x78, 0x3C, 0x3F,
           0x7F, 0x7F, 0x33, 0x01, 0x01, 0x00, 0x00, 0x80, 0xE0, 0xFF, 0xCF, 0xDF, 0xFF, 0xFF, 0xFE, 0xFC,
           0xF8, 0xF0, 0xF0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE1, 0xE1, 0xE1, 0xE1, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0,
           0xF0, 0xF8, 0xFC, 0xFC, 0xFF, 0xFF, 0xDF, 0xCF} // Setting
};

// 左轮PID菜单选项
Option left_wheel_options[] = {
    {.text = (char *)"- Left Wheel PID"},
    {.text = (char *)"$ P Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ I Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ D Parameter", .val = 0, .decimalNum = DecimalNum_2}};

// 右轮PID菜单选项
Option right_wheel_options[] = {
    {.text = (char *)"- Right Wheel PID"},
    {.text = (char *)"$ P Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ I Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ D Parameter", .val = 0, .decimalNum = DecimalNum_2}};

// 主菜单回调函数
bool MainMenu_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  if (msg_click == msg)
  {
    Option *select_item = WouoUI_ListTitlePageGetSelectOpt(cur_page_addr);
    if (!strcmp(select_item->content, "Left"))
    {
      // 更新左轮菜单的参数值
      left_wheel_options[1].val = (int32_t)(left_wheel_pid.p * 100);
      left_wheel_options[2].val = (int32_t)(left_wheel_pid.i * 100);
      left_wheel_options[3].val = (int32_t)(left_wheel_pid.d * 100);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &left_wheel_menu);
    }
    else if (!strcmp(select_item->content, "Right"))
    {
      // 更新右轮菜单的参数值
      right_wheel_options[1].val = (int32_t)(right_wheel_pid.p * 100);
      right_wheel_options[2].val = (int32_t)(right_wheel_pid.i * 100);
      right_wheel_options[3].val = (int32_t)(right_wheel_pid.d * 100);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &right_wheel_menu);
    }
  }
  return false;
}

// 左轮菜单回调函数
bool LeftWheelMenu_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  if (msg_click == msg)
  {
    Option *select_item = WouoUI_ListTitlePageGetSelectOpt(cur_page_addr);
    if (select_item->order >= 1 && select_item->order <= 3)
    {
      // 根据选择的参数类型设置范围和步长
      int min_val, max_val, step;
      switch (select_item->order)
      {
      case 1: // P Parameter
        min_val = PID_PARAM_P_MIN;
        max_val = PID_PARAM_P_MAX;
        step = PID_PARAM_P_STEP;
        break;
      case 2: // I Parameter
        min_val = PID_PARAM_I_MIN;
        max_val = PID_PARAM_I_MAX;
        step = PID_PARAM_I_STEP;
        break;
      case 3: // D Parameter
        min_val = PID_PARAM_D_MIN;
        max_val = PID_PARAM_D_MAX;
        step = PID_PARAM_D_STEP;
        break;
      default: // 不应该发生
        return false;
      }

      // 设置 ValWin 参数并跳转
      WouoUI_ValWinPageSetMinStepMax(&param_adjust_val_win, min_val, step, max_val);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &param_adjust_val_win);
    }
  }
  return false;
}

// 右轮菜单回调函数
bool RightWheelMenu_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  if (msg_click == msg)
  {
    Option *select_item = WouoUI_ListTitlePageGetSelectOpt(cur_page_addr);
    if (select_item->order >= 1 && select_item->order <= 3)
    {
      // 根据选择的参数类型设置范围和步长
      int min_val, max_val, step;
      switch (select_item->order)
      {
      case 1: // P Parameter
        min_val = PID_PARAM_P_MIN;
        max_val = PID_PARAM_P_MAX;
        step = PID_PARAM_P_STEP;
        break;
      case 2: // I Parameter
        min_val = PID_PARAM_I_MIN;
        max_val = PID_PARAM_I_MAX;
        step = PID_PARAM_I_STEP;
        break;
      case 3: // D Parameter
        min_val = PID_PARAM_D_MIN;
        max_val = PID_PARAM_D_MAX;
        step = PID_PARAM_D_STEP;
        break;
      default: // 不应该发生
        return false;
      }

      // 设置 ValWin 参数并跳转
      WouoUI_ValWinPageSetMinStepMax(&param_adjust_val_win, min_val, step, max_val);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &param_adjust_val_win);
    }
  }
  return false;
}

// 参数调整弹窗回调函数 (修改为处理 ValWin)
bool ParamAdjust_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  // 获取 ValWin 实例和其父页面 (菜单)
  ValWin *val_win = (ValWin *)cur_page_addr;
  Page *parent = (Page *)val_win->page.last_page;
  Option *select_opt = WouoUI_ListTitlePageGetSelectOpt(parent); // 获取来源菜单选中的选项

  // 根据输入消息调整数值
  if (msg == msg_up || msg == msg_right)
  { // 假设上/右是增加
    WouoUI_ValWinPageValIncrease(val_win);
    return true; // 消息已处理，阻止页面自动返回
  }
  else if (msg == msg_down || msg == msg_left)
  { // 假设下/左是减少
    WouoUI_ValWinPageValDecrease(val_win);
    return true; // 消息已处理，阻止页面自动返回
  }
  else if (msg == msg_click)
  { // 点击确认
    // 判断是左轮还是右轮菜单
    if (parent == (Page *)&left_wheel_menu)
    {
      // 更新左轮PID参数 (注意 val 是 int32_t，需转为 float)
      switch (select_opt->order)
      {
      case 1:
        left_wheel_pid.p = val_win->val / 100.0f;
        break;
      case 2:
        left_wheel_pid.i = val_win->val / 100.0f;
        break;
      case 3:
        left_wheel_pid.d = val_win->val / 100.0f;
        break;
      }
      // 更新菜单显示值 (ValWin 开启了 auto_set_bg_opt, 会自动更新，这里无需手动更新)
      // left_wheel_options[select_opt->order].val = val_win->val; // 理论上不需要
    }
    else if (parent == (Page *)&right_wheel_menu)
    {
      // 更新右轮PID参数
      switch (select_opt->order)
      {
      case 1:
        right_wheel_pid.p = val_win->val / 100.0f;
        break;
      case 2:
        right_wheel_pid.i = val_win->val / 100.0f;
        break;
      case 3:
        right_wheel_pid.d = val_win->val / 100.0f;
        break;
      }
      // 更新菜单显示值 (ValWin 开启了 auto_set_bg_opt, 会自动更新，这里无需手动更新)
      // right_wheel_options[select_opt->order].val = val_win->val; // 理论上不需要
    }
    // 确认后，不需要 return true，让页面自动返回
  }
  // 对于 msg_return 或其他未处理的消息，返回 false，让页面自动处理返回逻辑
  return false;
}

void PIDMenu_Init(void)
{
  // 选择默认UI
  WouoUI_SelectDefaultUI();

  // 清空缓存并刷新屏幕
  WouoUI_BuffClear();
  WouoUI_BuffSend();
  WouoUI_GraphSetPenColor(1);

  // 初始化菜单选项的值
  left_wheel_options[1].val = (int32_t)(left_wheel_pid.p * 100);
  left_wheel_options[2].val = (int32_t)(left_wheel_pid.i * 100);
  left_wheel_options[3].val = (int32_t)(left_wheel_pid.d * 100);

  right_wheel_options[1].val = (int32_t)(right_wheel_pid.p * 100);
  right_wheel_options[2].val = (int32_t)(right_wheel_pid.i * 100);
  right_wheel_options[3].val = (int32_t)(right_wheel_pid.d * 100);

  // 初始化页面对象
  WouoUI_TitlePageInit(&main_menu, MAIN_MENU_NUM, main_menu_options, main_menu_icons, MainMenu_CallBack);
  WouoUI_ListPageInit(&left_wheel_menu, sizeof(left_wheel_options) / sizeof(Option), left_wheel_options, Setting_none, LeftWheelMenu_CallBack);
  WouoUI_ListPageInit(&right_wheel_menu, sizeof(right_wheel_options) / sizeof(Option), right_wheel_options, Setting_none, RightWheelMenu_CallBack);
  // WouoUI_SpinWinPageInit(&param_adjust_win, NULL, 0, DecimalNum_2, 0, 1000, true, true, ParamAdjust_CallBack); // 移除 SpinWin 初始化
  // 初始化 ValWin 页面
  // text: NULL (将由 auto_get_bg_opt 自动设置)
  // init_val: 0 (将由 auto_get_bg_opt 自动设置)
  // min, max, step: 将在跳转前由 WouoUI_ValWinPageSetMinStepMax 设置
  // auto_get_bg_opt: true (自动获取父页面选项的 text 和 val)
  // auto_set_bg_opt: true (确认时自动将 val 写回父页面选项)
  // cb: ParamAdjust_CallBack (使用同一个回调函数)
  WouoUI_ValWinPageInit(&param_adjust_val_win, NULL, 0, 0, 1000, 10, true, true, ParamAdjust_CallBack);
}

/* Oled 显示任务 */
void oled_task(void)
{
  //  // --- 准备阶段 ---
  //  // 设置绘图颜色 (对于单色屏，1 通常表示点亮像素)
  //  u8g2_SetDrawColor(&u8g2, 1);
  //  // 选择要使用的字体 (确保字体文件已添加到工程)
  //  u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr); // ncenB08: 字体名, _tr: 透明背景

  //  // --- 核心绘图流程 ---
  //  // 1. 清除内存缓冲区 (非常重要，每次绘制新帧前必须调用)
  //  u8g2_ClearBuffer(&u8g2);

  //  // 2. 使用 u8g2 API 在缓冲区中绘图
  //  //    所有绘图操作都作用于 RAM 中的缓冲区。
  //  // 绘制字符串 (参数: u8g2实例, x坐标, y坐标, 字符串)
  //  // y 坐标通常是字符串基线的位置。
  //  u8g2_DrawStr(&u8g2, 2, 12, "Hello u8g2!"); // 从 (2, 12) 开始绘制
  //  u8g2_DrawStr(&u8g2, 2, 28, "Micron Elec Studio"); // 绘制第二行

  //  // 绘制图形 (示例：一个空心圆和一个实心框)
  //  // 绘制圆 (参数: u8g2实例, 圆心x, 圆心y, 半径, 绘制选项)
  //  u8g2_DrawCircle(&u8g2, 90, 19, 10, U8G2_DRAW_ALL); // U8G2_DRAW_ALL 画圆周
  //  // 绘制实心框 (参数: u8g2实例, 左上角x, 左上角y, 宽度, 高度)
  //  // u8g2_DrawBox(&u8g2, 50, 15, 20, 10);
  //  // 绘制空心框 (参数: u8g2实例, 左上角x, 左上角y, 宽度, 高度)
  //  // u8g2_DrawFrame(&u8g2, 50, 15, 20, 10);

  //  // 3. 将缓冲区内容一次性发送到屏幕 (非常重要)
  //  //    这个函数会调用我们之前编写的 I2C 回调函数，将整个缓冲区的数据发送出去。
  //  u8g2_SendBuffer(&u8g2);
  WouoUI_Proc(10); // 假设任务每 10ms 调用一次
}
