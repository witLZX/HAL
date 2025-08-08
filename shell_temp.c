#include "uart_app.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> // ���ڴ����ַ���ת��Ϊ����

#define BUUFER_SIZE 64
#define SHELL_MAX_COMMAND_LENGTH 64
#define SHELL_HISTORY_SIZE 10 // ��ʷ����������

// ���廷�λ������ͽ��ջ�����
ringbuffer_t usart_rb;
uint8_t usart_read_buffer[BUUFER_SIZE];

// ��������Shell���������
char shell_command_buffer[SHELL_MAX_COMMAND_LENGTH];
int shell_command_index = 0;

// ����������ʷ��¼
char shell_history[SHELL_HISTORY_SIZE][SHELL_MAX_COMMAND_LENGTH];
int shell_history_index = 0;    // ��ǰ��ʷ��¼����
int shell_history_current = -1; // ���¼�ʹ��ʱ�ĵ�ǰѡ������

// ESC����־�����ڼ�����¼���������
int esc_seq_flag = 0;

// �����ͨ��paraset�������õ�ȫ�ֱ���
int vara = 0;
int varb = 0;
int varc = 0;

// ����Shell֧�ֵ�����ṹ
typedef void (*shell_command_function_t)(void);

typedef struct
{
    const char *name;
    shell_command_function_t function;
} shell_command_t;

// ������Զ���ȫ�ı���������
const char *shell_variables[] =
    {
        "vara",
        "varb",
        "varc",
};

// �����������
void shell_cmd_help(void);
void shell_cmd_version(void);
void shell_cmd_paraset(void);
void shell_execute_command(void);
void shell_add_to_history(const char *command);
void shell_tab_complete(void);
void shell_browse_history(int direction);

// ����֧�ֵ�����
shell_command_t shell_commands[] =
    {
        {"help", shell_cmd_help},
        {"version", shell_cmd_version},
        {"paraset", shell_cmd_paraset},
};

// UART DMA������ɻص������������յ�������д�뻷�λ�����
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (!ringbuffer_is_full(&usart_rb))
    {
        ringbuffer_write(&usart_rb, uart_rx_dma_buffer, Size);
    }
    memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));
}

// ����UART���ջ������е�����
void uart_proc(void)
{
    if (ringbuffer_is_empty(&usart_rb)) return;

    uint16_t available_data = usart_rb.itemCount;
    ringbuffer_read(&usart_rb, usart_read_buffer, usart_rb.itemCount);

    for (int i = 0; i < available_data; i++)
    {
        char ch = usart_read_buffer[i];

        if (ch == '\r' || ch == '\n') // �س����б�ʾ�����������
        {
            shell_command_buffer[shell_command_index] = '\0'; // ����ַ���������
            shell_execute_command();                          // ������ִ������
            shell_add_to_history(shell_command_buffer);       // ��������ӵ���ʷ��¼
            shell_command_index = 0;                          // ���������������
            shell_history_current = -1;                       // ������ʷ����ѡ��
        }
        else if (ch == '\b' && shell_command_index > 0) // �����˸��
        {
            shell_command_index--;
            printf("\b \b"); // �˸�ɾ����Ļ�ϵ��ַ�
        }
        else if (ch == '\t') // ����TAB����ȫ����
        {
            shell_tab_complete();
        }
        else if (ch == 27) // ���ESC����ͷ�Ŀ�������
        {
            esc_seq_flag = 1; // ����ESC���б�־
        }
        else if (esc_seq_flag == 1 && ch == '[') // ��⵽ESC���[
        {
            esc_seq_flag = 2; // ��⵽����ESC���е��м䲿��
        }
        else if (esc_seq_flag == 2) // ���ESC���еļ��
        {
            if (ch == 'A') // �ϼ�
            {
                shell_browse_history(-1); // ��һ����ʷ����
            }
            else if (ch == 'B') // �¼�
            {
                shell_browse_history(1); // ��һ����ʷ����
            }
            esc_seq_flag = 0; // ��λESC��־
        }
        else if (ch >= 32 && ch < 127 && shell_command_index < SHELL_MAX_COMMAND_LENGTH - 1) // ��ӡ�ɼ��ַ�
        {
            shell_command_buffer[shell_command_index++] = ch; // �����ַ����������
            printf("%c", ch);                                 // ʵʱ��ʾ������ַ�
        }
    }

    // ��ն�ȡ������
    memset(usart_read_buffer, 0, sizeof(uint8_t) * BUUFER_SIZE);
}

// Shell����ִ�к���
void shell_execute_command(void)
{
    printf("\r\n"); // ȷ������ִ�н���������������ķָ�
    for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
    {
        if (strncmp(shell_command_buffer, shell_commands[i].name, strlen(shell_commands[i].name)) == 0)
        {
            shell_commands[i].function(); // ִ�ж�Ӧ�������
            printf("\r\n> ");             // ִ��������ӡ��ʾ��������
            return;
        }
    }
    printf("Unknown command: %s\r\n", shell_command_buffer);
    printf("> "); // ��ӡ��ʾ��
}

// ��������ӵ���ʷ��¼
void shell_add_to_history(const char *command)
{
    if (command[0] != '\0') // ȷ���ǿ��������ӵ���ʷ��¼
    {
        strcpy(shell_history[shell_history_index], command);
        shell_history_index = (shell_history_index + 1) % SHELL_HISTORY_SIZE; // ���δ洢
    }
}

// TAB��ȫ���ܣ����ӱ����Զ���ȫ����
void shell_tab_complete(void)
{
    int match_count = 0;
    const char *match = NULL;
    int cmd_length = 0;

    // ��鵱ǰ����������Ǳ���
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

    // ���ֻƥ�䵽һ���������������в�ȫ
    if (match_count == 1 && match != NULL)
    {
        strcpy(shell_command_buffer + cmd_length, match);
        shell_command_index = strlen(shell_command_buffer);
        printf("\r\033[K> %s", shell_command_buffer); // �����ǰ�в���ʾ��ȫ����������
    }
    else if (match_count > 1) // ���ƥ�䵽���������������ʾ����ƥ����
    {
        printf("\r\n");      // ������ʾ����ƥ�����������
        if (cmd_length == 0) // ���ȫ
        {
            for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
            {
                if (strncmp(shell_command_buffer, shell_commands[i].name, shell_command_index) == 0)
                {
                    printf("%s  ", shell_commands[i].name);
                }
            }
        }
        else // ������ȫ
        {
            for (int i = 0; i < sizeof(shell_variables) / sizeof(char *); i++)
            {
                if (strncmp(shell_command_buffer + cmd_length, shell_variables[i], shell_command_index - cmd_length) == 0)
                {
                    printf("%s  ", shell_variables[i]);
                }
            }
        }
        printf("\r\n> %s", shell_command_buffer); // ������ʾ��ʾ���͵�ǰ����
    }
}

// �����ʷ����
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
        printf("%s", shell_command_buffer); // ��ʾ��ʷ����
    }
    else
    {
        shell_command_buffer[0] = '\0'; // ����������
        shell_command_index = 0;
    }
}

// Shell����: help
void shell_cmd_help(void)
{
    printf("Available commands:\r\n");
    for (int i = 0; i < sizeof(shell_commands) / sizeof(shell_command_t); i++)
    {
        printf("- %s\r\n", shell_commands[i].name);
    }
}

// Shell����: version
void shell_cmd_version(void)
{
    printf("Simple Shell Version 1.0\r\n");
}

// Shell����: paraset
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

// Shell��ʼ������
void shell_init(void)
{
    shell_command_index = 0;
    printf("Simple Shell initialized.\r\n");
    printf("> "); // ��ӡ��ʾ��
}
