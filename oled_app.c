#include "oled_app.h"

// PID ������Χ�Ͳ����궨��
#define PID_PARAM_P_MIN 0    // P ������Сֵ (0.00)
#define PID_PARAM_P_MAX 1000 // P �������ֵ (10.00)
#define PID_PARAM_P_STEP 10  // P ��������ֵ (0.10)

#define PID_PARAM_I_MIN 0    // I ������Сֵ (0.00)
#define PID_PARAM_I_MAX 1000 // I �������ֵ (10.00)
#define PID_PARAM_I_STEP 10  // I ��������ֵ (0.10)

#define PID_PARAM_D_MIN 0   // D ������Сֵ (0.00)
#define PID_PARAM_D_MAX 100 // D �������ֵ (1.00)
#define PID_PARAM_D_STEP 1  // D ��������ֵ (0.01)

/**
 * @brief	ʹ������printf�ķ�ʽ��ʾ�ַ�������ʾ6x8��С��ASCII�ַ�
 * @param x  Character position on the X-axis  range��0 - 127
 * @param y  Character position on the Y-axis  range��0 - 3
 * ���磺oled_printf(0, 0, "Data = %d", dat);
 **/
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
  char buffer[512]; // ��ʱ�洢��ʽ������ַ���
  va_list arg;      // ����ɱ����
  int len;          // �����ַ�������

  va_start(arg, format);
  // ��ȫ�ظ�ʽ���ַ����� buffer
  len = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  OLED_ShowStr(x, y, buffer, 8);
  return len;
}

// u8g2 �� GPIO ����ʱ�ص�����
uint8_t u8g2_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch (msg)
  {
  case U8X8_MSG_GPIO_AND_DELAY_INIT:
    // ��ʼ�� GPIO (�����Ҫ������ SPI �� CS, DC, RST ����)
    // ����Ӳ�� I2C������ͨ������Ҫ��ʲô
    break;
  case U8X8_MSG_DELAY_MILLI:
    // ԭ��: u8g2 �ڲ�ĳЩ������Ҫ���뼶����ʱ�ȴ���
    // �ṩ���뼶��ʱ��ֱ�ӵ��� HAL �⺯����
    HAL_Delay(arg_int);
    break;
  case U8X8_MSG_DELAY_10MICRO:
    // ʵ��10΢����ʱ��ʹ�þ�ȷУ׼�Ŀ�ѭ��
    {
      // GD32ϵ��ͨ�������ٶ�Ϊ120-200MHz��ÿ��ѭ����Լ��Ҫ3-4��ʱ������
      // ��160MHz���㣬10��s��ҪԼ400-500��ѭ��
      for (volatile uint32_t i = 0; i < 480; i++)
      {
        __NOP(); // �����������Ż������ָ��
      }
    }
    break;
  case U8X8_MSG_DELAY_100NANO:
    // ʵ��100������ʱ��ʹ�ö��NOPָ��
    // ÿ��NOPָ���Լ��Ҫ1��ʱ������(Լ6ns@160MHz)
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
  case U8X8_MSG_GPIO_I2C_CLOCK: // [[fallthrough]] // Fallthrough ע�ͱ�ʾ����Ϊ֮
  case U8X8_MSG_GPIO_I2C_DATA:
    // ���� SCL/SDA ���ŵ�ƽ����Щ����**���ģ�� I2C** ʱ��Ҫʵ�֡�
    // ʹ��Ӳ�� I2C ʱ����Щ��Ϣ���Ժ��ԣ��� HAL �⴦��
    break;
  // --- ������ GPIO ��ص���Ϣ����Ҫ���ڰ�������� SPI ���� ---
  // ������ u8g2 Ӧ����Ҫ��ȡ��������� SPI ���� (CS, DC, Reset)��
  // ����Ҫ��������� msg ���Ͷ�ȡ/���ö�Ӧ�� GPIO ����״̬��
  // ���ڽ�ʹ��Ӳ�� I2C ��ʾ�ĳ��������������������򵥷��ز�֧�֡�
  case U8X8_MSG_GPIO_CS:
    // SPI Ƭѡ����
    break;
  case U8X8_MSG_GPIO_DC:
    // SPI ����/�����߿���
    break;
  case U8X8_MSG_GPIO_RESET:
    // ��ʾ����λ���ſ���
    break;
  case U8X8_MSG_GPIO_MENU_SELECT:
    u8x8_SetGPIOResult(u8x8, /* ��ȡѡ��� GPIO ״̬ */ 0);
    break;
  default:
    u8x8_SetGPIOResult(u8x8, 1); // ��֧�ֵ���Ϣ
    break;
  }
  return 1;
}

// u8g2 ��Ӳ�� I2C ͨ�Żص�����
uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  static uint8_t buffer[32]; // u8g2 ÿ�δ������ 32 �ֽ�
  static uint8_t buf_idx;
  uint8_t *data;

  switch (msg)
  {
  case U8X8_MSG_BYTE_SEND:
    // ԭ��: u8g2 ͨ������һ���Է��ʹ������ݣ����Ƿֿ鷢�͡�
    // �����Ϣ���ڽ�һС������ (arg_int �ֽ�) �� u8g2 �ڲ����ݵ����ǵĻص�������
    // ������Ҫ����Щ�����ݴ浽���� buffer �У��ȴ� START/END_TRANSFER �źš�
    data = (uint8_t *)arg_ptr;
    while (arg_int > 0)
    {
      buffer[buf_idx++] = *data;
      data++;
      arg_int--;
    }
    break;
  case U8X8_MSG_BYTE_INIT:
    // ԭ��: �ṩһ��������� I2C ����ĳ�ʼ����
    // ��ʼ�� I2C (ͨ���� main �����������)
    // ���������� main �������Ѿ������� MX_I2C1_Init()������ͨ���������ա�
    break;
  case U8X8_MSG_BYTE_SET_DC:
    // ԭ��: �����Ϣ���� SPI ͨ���п��� Data/Command ѡ�����š�
    // ��������/������ (I2C ����Ҫ)
    // I2C ͨ���ض��Ŀ����ֽ� (0x00 �� 0x40) ������������ݣ���˸���Ϣ���� I2C �����塣
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    // ԭ��: ���һ�� I2C �������еĿ�ʼ��
    buf_idx = 0;
    // �������������ñ��ػ�������������׼�������µ����ݿ顣
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    // ԭ��: ���һ�� I2C �������еĽ�����
    // ��ʱ������ buffer ���Ѿ��ݴ���������Ҫ���͵����ݿ顣
    // ����ִ��ʵ�� I2C ���Ͳ��������ʱ����
    // ���ͻ������е�����
    // ע��: u8x8_GetI2CAddress(u8x8) ���ص��� 7 λ��ַ * 2 = 8 λ��ַ
    if (HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buffer, buf_idx, 100) != HAL_OK)
    {
      return 0; // ����ʧ��
    }
    break;
  default:
    return 0;
  }
  return 1;
}

/* ����ˢ�º��� */
void OLED_SendBuff(uint8_t buff[4][128])
{
  // ��ȡ u8g2 �Ļ�����ָ��
  uint8_t *u8g2_buffer = u8g2_GetBufferPtr(&u8g2);

  // �����ݿ����� u8g2 �Ļ�����
  memcpy(u8g2_buffer, buff, 4 * 128);

  // ���������������� OLED
  u8g2_SendBuffer(&u8g2);
}

// ҳ�����
TitlePage main_menu;         // ��һ���˵���������ѡ��
ListPage left_wheel_menu;    // ���ֵ�PID����ѡ��˵�
ListPage right_wheel_menu;   // ���ֵ�PID����ѡ��˵�
ValWin param_adjust_val_win; // ʹ�� ValWin ���в�������

// ������PID��������
typedef struct
{
  float p;
  float i;
  float d;
} PIDParams;

PIDParams left_wheel_pid = {1.0, 0.1, 0.01};
PIDParams right_wheel_pid = {1.0, 0.1, 0.01};

// ��һ���˵�ѡ�������ѡ��
#define MAIN_MENU_NUM 2
Option main_menu_options[MAIN_MENU_NUM] = {
    {.text = (char *)"+ Left Wheel", .content = (char *)"Left"},
    {.text = (char *)"+ Right Wheel", .content = (char *)"Right"}};

// ��һ���˵�ͼ��
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

// ����PID�˵�ѡ��
Option left_wheel_options[] = {
    {.text = (char *)"- Left Wheel PID"},
    {.text = (char *)"$ P Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ I Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ D Parameter", .val = 0, .decimalNum = DecimalNum_2}};

// ����PID�˵�ѡ��
Option right_wheel_options[] = {
    {.text = (char *)"- Right Wheel PID"},
    {.text = (char *)"$ P Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ I Parameter", .val = 0, .decimalNum = DecimalNum_2},
    {.text = (char *)"$ D Parameter", .val = 0, .decimalNum = DecimalNum_2}};

// ���˵��ص�����
bool MainMenu_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  if (msg_click == msg)
  {
    Option *select_item = WouoUI_ListTitlePageGetSelectOpt(cur_page_addr);
    if (!strcmp(select_item->content, "Left"))
    {
      // �������ֲ˵��Ĳ���ֵ
      left_wheel_options[1].val = (int32_t)(left_wheel_pid.p * 100);
      left_wheel_options[2].val = (int32_t)(left_wheel_pid.i * 100);
      left_wheel_options[3].val = (int32_t)(left_wheel_pid.d * 100);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &left_wheel_menu);
    }
    else if (!strcmp(select_item->content, "Right"))
    {
      // �������ֲ˵��Ĳ���ֵ
      right_wheel_options[1].val = (int32_t)(right_wheel_pid.p * 100);
      right_wheel_options[2].val = (int32_t)(right_wheel_pid.i * 100);
      right_wheel_options[3].val = (int32_t)(right_wheel_pid.d * 100);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &right_wheel_menu);
    }
  }
  return false;
}

// ���ֲ˵��ص�����
bool LeftWheelMenu_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  if (msg_click == msg)
  {
    Option *select_item = WouoUI_ListTitlePageGetSelectOpt(cur_page_addr);
    if (select_item->order >= 1 && select_item->order <= 3)
    {
      // ����ѡ��Ĳ����������÷�Χ�Ͳ���
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
      default: // ��Ӧ�÷���
        return false;
      }

      // ���� ValWin ��������ת
      WouoUI_ValWinPageSetMinStepMax(&param_adjust_val_win, min_val, step, max_val);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &param_adjust_val_win);
    }
  }
  return false;
}

// ���ֲ˵��ص�����
bool RightWheelMenu_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  if (msg_click == msg)
  {
    Option *select_item = WouoUI_ListTitlePageGetSelectOpt(cur_page_addr);
    if (select_item->order >= 1 && select_item->order <= 3)
    {
      // ����ѡ��Ĳ����������÷�Χ�Ͳ���
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
      default: // ��Ӧ�÷���
        return false;
      }

      // ���� ValWin ��������ת
      WouoUI_ValWinPageSetMinStepMax(&param_adjust_val_win, min_val, step, max_val);
      WouoUI_JumpToPage((PageAddr)cur_page_addr, &param_adjust_val_win);
    }
  }
  return false;
}

// �������������ص����� (�޸�Ϊ���� ValWin)
bool ParamAdjust_CallBack(const Page *cur_page_addr, InputMsg msg)
{
  // ��ȡ ValWin ʵ�����丸ҳ�� (�˵�)
  ValWin *val_win = (ValWin *)cur_page_addr;
  Page *parent = (Page *)val_win->page.last_page;
  Option *select_opt = WouoUI_ListTitlePageGetSelectOpt(parent); // ��ȡ��Դ�˵�ѡ�е�ѡ��

  // ����������Ϣ������ֵ
  if (msg == msg_up || msg == msg_right)
  { // ������/��������
    WouoUI_ValWinPageValIncrease(val_win);
    return true; // ��Ϣ�Ѵ�����ֹҳ���Զ�����
  }
  else if (msg == msg_down || msg == msg_left)
  { // ������/���Ǽ���
    WouoUI_ValWinPageValDecrease(val_win);
    return true; // ��Ϣ�Ѵ�����ֹҳ���Զ�����
  }
  else if (msg == msg_click)
  { // ���ȷ��
    // �ж������ֻ������ֲ˵�
    if (parent == (Page *)&left_wheel_menu)
    {
      // ��������PID���� (ע�� val �� int32_t����תΪ float)
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
      // ���²˵���ʾֵ (ValWin ������ auto_set_bg_opt, ���Զ����£����������ֶ�����)
      // left_wheel_options[select_opt->order].val = val_win->val; // �����ϲ���Ҫ
    }
    else if (parent == (Page *)&right_wheel_menu)
    {
      // ��������PID����
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
      // ���²˵���ʾֵ (ValWin ������ auto_set_bg_opt, ���Զ����£����������ֶ�����)
      // right_wheel_options[select_opt->order].val = val_win->val; // �����ϲ���Ҫ
    }
    // ȷ�Ϻ󣬲���Ҫ return true����ҳ���Զ�����
  }
  // ���� msg_return ������δ�������Ϣ������ false����ҳ���Զ��������߼�
  return false;
}

void PIDMenu_Init(void)
{
  // ѡ��Ĭ��UI
  WouoUI_SelectDefaultUI();

  // ��ջ��沢ˢ����Ļ
  WouoUI_BuffClear();
  WouoUI_BuffSend();
  WouoUI_GraphSetPenColor(1);

  // ��ʼ���˵�ѡ���ֵ
  left_wheel_options[1].val = (int32_t)(left_wheel_pid.p * 100);
  left_wheel_options[2].val = (int32_t)(left_wheel_pid.i * 100);
  left_wheel_options[3].val = (int32_t)(left_wheel_pid.d * 100);

  right_wheel_options[1].val = (int32_t)(right_wheel_pid.p * 100);
  right_wheel_options[2].val = (int32_t)(right_wheel_pid.i * 100);
  right_wheel_options[3].val = (int32_t)(right_wheel_pid.d * 100);

  // ��ʼ��ҳ�����
  WouoUI_TitlePageInit(&main_menu, MAIN_MENU_NUM, main_menu_options, main_menu_icons, MainMenu_CallBack);
  WouoUI_ListPageInit(&left_wheel_menu, sizeof(left_wheel_options) / sizeof(Option), left_wheel_options, Setting_none, LeftWheelMenu_CallBack);
  WouoUI_ListPageInit(&right_wheel_menu, sizeof(right_wheel_options) / sizeof(Option), right_wheel_options, Setting_none, RightWheelMenu_CallBack);
  // WouoUI_SpinWinPageInit(&param_adjust_win, NULL, 0, DecimalNum_2, 0, 1000, true, true, ParamAdjust_CallBack); // �Ƴ� SpinWin ��ʼ��
  // ��ʼ�� ValWin ҳ��
  // text: NULL (���� auto_get_bg_opt �Զ�����)
  // init_val: 0 (���� auto_get_bg_opt �Զ�����)
  // min, max, step: ������תǰ�� WouoUI_ValWinPageSetMinStepMax ����
  // auto_get_bg_opt: true (�Զ���ȡ��ҳ��ѡ��� text �� val)
  // auto_set_bg_opt: true (ȷ��ʱ�Զ��� val д�ظ�ҳ��ѡ��)
  // cb: ParamAdjust_CallBack (ʹ��ͬһ���ص�����)
  WouoUI_ValWinPageInit(&param_adjust_val_win, NULL, 0, 0, 1000, 10, true, true, ParamAdjust_CallBack);
}

/* Oled ��ʾ���� */
void oled_task(void)
{
  //  // --- ׼���׶� ---
  //  // ���û�ͼ��ɫ (���ڵ�ɫ����1 ͨ����ʾ��������)
  //  u8g2_SetDrawColor(&u8g2, 1);
  //  // ѡ��Ҫʹ�õ����� (ȷ�������ļ�����ӵ�����)
  //  u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr); // ncenB08: ������, _tr: ͸������

  //  // --- ���Ļ�ͼ���� ---
  //  // 1. ����ڴ滺���� (�ǳ���Ҫ��ÿ�λ�����֡ǰ�������)
  //  u8g2_ClearBuffer(&u8g2);

  //  // 2. ʹ�� u8g2 API �ڻ������л�ͼ
  //  //    ���л�ͼ������������ RAM �еĻ�������
  //  // �����ַ��� (����: u8g2ʵ��, x����, y����, �ַ���)
  //  // y ����ͨ�����ַ������ߵ�λ�á�
  //  u8g2_DrawStr(&u8g2, 2, 12, "Hello u8g2!"); // �� (2, 12) ��ʼ����
  //  u8g2_DrawStr(&u8g2, 2, 28, "Micron Elec Studio"); // ���Ƶڶ���

  //  // ����ͼ�� (ʾ����һ������Բ��һ��ʵ�Ŀ�)
  //  // ����Բ (����: u8g2ʵ��, Բ��x, Բ��y, �뾶, ����ѡ��)
  //  u8g2_DrawCircle(&u8g2, 90, 19, 10, U8G2_DRAW_ALL); // U8G2_DRAW_ALL ��Բ��
  //  // ����ʵ�Ŀ� (����: u8g2ʵ��, ���Ͻ�x, ���Ͻ�y, ���, �߶�)
  //  // u8g2_DrawBox(&u8g2, 50, 15, 20, 10);
  //  // ���ƿ��Ŀ� (����: u8g2ʵ��, ���Ͻ�x, ���Ͻ�y, ���, �߶�)
  //  // u8g2_DrawFrame(&u8g2, 50, 15, 20, 10);

  //  // 3. ������������һ���Է��͵���Ļ (�ǳ���Ҫ)
  //  //    ����������������֮ǰ��д�� I2C �ص������������������������ݷ��ͳ�ȥ��
  //  u8g2_SendBuffer(&u8g2);
  WouoUI_Proc(10); // ��������ÿ 10ms ����һ��
}
