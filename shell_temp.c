#include "uart_app.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> // 用于处理字符串转换为数字

#define BUUFER_SIZE 64
#define SHELL_MAX_COMMAND_LENGTH 64
#define SHELL_HISTORY_SIZE 10 // 历史命令保存的条数

// 定义环形缓冲区和接收缓冲区
ringbuffer_t usart_rb;
uint8_t usart_read_buffer[BUUFER_SIZE];

// 定义用于Shell的命令缓冲区
char shell_command_buffer[SHELL_MAX_COMMAND_LENGTH];
int shell_command_index = 0;

// 定义命令历史记录
char shell_history[SHELL_HISTORY_SIZE][SHELL_MAX_COMMAND_LENGTH];
int shell_history_index = 0;    // 当前历史记录索引
int shell_history_current = -1; // 上下键使用时的当前选择索引

// ESC键标志，用于检测上下键完整序列
int esc_seq_flag = 0;

// 定义可通过paraset命令设置的全局变量
int vara = 0;
int varb = 0;
int varc = 0;

// 定义Shell支持的命令结构
typedef void (*shell_command_function_t)(void);

typedef struct
{
    const char *name;
    shell_command_function_t function;
} shell_command_t;

// 定义可自动补全的变量名数组
const char *shell_variables[] =
    {
        "vara",
        "varb",
        "varc",
};

// 命令处理函数声明
void shell_cmd_help(void);
void shell_cmd_version(void);
void shell_cmd_paraset(void);
void shell_execute_command(void);
void shell_add_to_history(const char *command);
void shell_tab_complete(void);
void shell_browse_history(int direction);

// 定义支持的命令
shell_command_t shell_commands[] =
    {
        {"help", shell_cmd_help},
        {"version", shell_cmd_version},
        {"paraset", shell_cmd_paraset},
};

// UART DMA接收完成回调函数，将接收到的数据写入环形缓冲区
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (!ringbuffer_is_full(&usart_rb))
    {
        ringbuffer_write(&usart_rb, uart_rx_dma_buffer, Size);
    }
    memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));
}

// 处理UART接收缓冲区中的数据
void uart_proc(void)
{
    if (ringbuffer_is_empty(&usart_rb)) return;

    uint16_t available_data = usart_rb.itemCount;
    ringbuffer_read(&usart_rb, usart_read_buffer, usart_rb.itemCount);

    for (int i = 0; i < available_data; i++)
    {
        char ch = usart_read_buffer[i];

        if (ch == '\r' || ch == '\n') // 回车或换行表示命令输入完成
        {
            shell_command_buffer[shell_command_index] = '\0'; // 添加字符串结束符
            shell_execute_command();                          // 解析并执行命令
            shell_add_to_history(shell_command_buffer);       // 将命令添加到历史记录
            shell_command_index = 0;                          // 重置命令缓冲区索引
            shell_history_current = -1;                       // 重置历史命令选择
        }
        else if (ch == '\b' && shell_command_index > 0) // 处理退格键
        {
            shell_command_index--;
            printf("\b \b"); // 退格并删除屏幕上的字符
        }
        else if (ch == '\t') // 处理TAB键补全功能
        {
            shell_tab_complete();
        }
        else if (ch == 27) // 检测ESC键开头的控制序列
        {
            esc_seq_flag = 1; // 设置ESC序列标志
        }
        else if (esc_seq_flag == 1 && ch == '[') // 检测到ESC后的[
        {
            esc_seq_flag = 2; // 检测到完整ESC序列的中间部分
        }
        else if (esc_seq_flag == 2) // 完成ESC序列的检测
        {
            if (ch == 'A') // 上键
            {
                shell_browse_history(-1); // 上一条历史命令
            }
            else if (ch == 'B') // 下键
            {
                shell_browse_history(1); // 下一条历史命令
            }
            esc_seq_flag = 0; // 复位ESC标志
        }
        else if (ch >= 32 && ch < 127 && shell_command_index < SHELL_MAX_COMMAND_LENGTH - 1) // 打印可见字符
        {
            shell_command_buffer[shell_command_index++] = ch; // 保存字符到命令缓冲区
            printf("%c", ch);                                 // 实时显示输入的字符
        }
    }

    // 清空读取缓冲区
    memset(usart_read_buffer, 0, sizeof(uint8_t) * BUUFER_SIZE);
}

// Shell命令执行函数
void shell_execute_command(void)
{
    printf("\r\n"); // 确保命令执行结果与输入有清晰的分隔
    for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
    {
        if (strncmp(shell_command_buffer, shell_commands[i].name, strlen(shell_commands[i].name)) == 0)
        {
            shell_commands[i].function(); // 执行对应的命令函数
            printf("\r\n> ");             // 执行命令后打印提示符并换行
            return;
        }
    }
    printf("Unknown command: %s\r\n", shell_command_buffer);
    printf("> "); // 打印提示符
}

// 将命令添加到历史记录
void shell_add_to_history(const char *command)
{
    if (command[0] != '\0') // 确保非空命令才添加到历史记录
    {
        strcpy(shell_history[shell_history_index], command);
        shell_history_index = (shell_history_index + 1) % SHELL_HISTORY_SIZE; // 环形存储
    }
}

// TAB补全功能，增加变量自动补全功能
void shell_tab_complete(void)
{
    int match_count = 0;
    const char *match = NULL;
    int cmd_length = 0;

    // 检查当前输入是命令还是变量
    if (strncmp(shell_command_buffer, "paraset ", 8) == 0)
    {
        cmd_length = 8;
        for (int i = 0; i < sizeof(shell_variables) / sizeof(char *); i++)
        {
            if (strncmp(shell_command_buffer + cmd_length, shell_variables[i], shell_command_index - cmd_length) == 0)
            {
                match = shell_variables[i];
                match_count++;
            }
        }
    }
    else
    {
        for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
        {
            if (strncmp(shell_command_buffer, shell_commands[i].name, shell_command_index) == 0)
            {
                match = shell_commands[i].name;
                match_count++;
            }
        }
    }

    // 如果只匹配到一个命令或变量，进行补全
    if (match_count == 1 && match != NULL)
    {
        strcpy(shell_command_buffer + cmd_length, match);
        shell_command_index = strlen(shell_command_buffer);
        printf("\r\033[K> %s", shell_command_buffer); // 清除当前行并显示补全的命令或变量
    }
    else if (match_count > 1) // 如果匹配到多个命令或变量，提示所有匹配项
    {
        printf("\r\n");      // 换行显示所有匹配的命令或变量
        if (cmd_length == 0) // 命令补全
        {
            for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
            {
                if (strncmp(shell_command_buffer, shell_commands[i].name, shell_command_index) == 0)
                {
                    printf("%s  ", shell_commands[i].name);
                }
            }
        }
        else // 变量补全
        {
            for (int i = 0; i < sizeof(shell_variables) / sizeof(char *); i++)
            {
                if (strncmp(shell_command_buffer + cmd_length, shell_variables[i], shell_command_index - cmd_length) == 0)
                {
                    printf("%s  ", shell_variables[i]);
                }
            }
        }
        printf("\r\n> %s", shell_command_buffer); // 重新显示提示符和当前命令
    }
}

// 浏览历史命令
void shell_browse_history(int direction)
{
    if (direction == -1 && shell_history_current < shell_history_index - 1)
    {
        shell_history_current++;
    }
    else if (direction == 1 && shell_history_current > 0)
    {
        shell_history_current--;
    }

    printf("\r\033[K> ");
    if (shell_history_current >= 0)
    {
        strcpy(shell_command_buffer, shell_history[shell_history_current]);
        shell_command_index = strlen(shell_command_buffer);
        printf("%s", shell_command_buffer); // 显示历史命令
    }
    else
    {
        shell_command_buffer[0] = '\0'; // 清空命令缓冲区
        shell_command_index = 0;
    }
}

// Shell命令: help
void shell_cmd_help(void)
{
    printf("Available commands:\r\n");
    for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
    {
        printf("- %s\r\n", shell_commands[i].name);
    }
}

// Shell命令: version
void shell_cmd_version(void)
{
    printf("Simple Shell Version 1.0\r\n");
}

// Shell命令: paraset
void shell_cmd_paraset(void)
{
    char var_name[16];
    int value;

    int parsed = sscanf(shell_command_buffer, "paraset %s %d", var_name, &value);
    if (parsed == 2)
    {
        if (strcmp(var_name, "vara") == 0)
        {
            vara = value;
            printf("vara set to %d\r\n", vara);
        }
        else if (strcmp(var_name, "varb") == 0)
        {
            varb = value;
            printf("varb set to %d\r\n", varb);
        }
        else if (strcmp(var_name, "varc") == 0)
        {
            varc = value;
            printf("varc set to %d\r\n", varc);
        }
        else
        {
            printf("Unknown variable: %s\r\n", var_name);
        }
    }
    else
    {
        printf("Usage: paraset <var_name> <value>\r\n");
    }
}

// Shell初始化函数
void shell_init(void)
{
    shell_command_index = 0;
    printf("Simple Shell initialized.\r\n");
    printf("> "); // 打印提示符
}
