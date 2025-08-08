#ifndef SHELL_APP_H
#define SHELL_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "mydefine.h"

// ����ѡ��
#define SHELL_MAX_COMMAND_LENGTH 64
#define SHELL_MAX_ARGS 8
#define SHELL_HISTORY_SIZE 10
#define SHELL_MAX_PATH_LEN 128
#define SHELL_MAX_LINE_LEN 80

// ����ص���������
typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

// ����ṹ��
typedef struct {
    const char *name;          // ��������
    const char *help;          // ������Ϣ
    shell_cmd_func_t function; // ִ�к���
} shell_command_t;

// Shell״̬
typedef struct {
    char cmd_buffer[SHELL_MAX_COMMAND_LENGTH];     // �������
    int cmd_len;                                   // �����
    char history[SHELL_HISTORY_SIZE][SHELL_MAX_COMMAND_LENGTH]; // ��ʷ����
    int history_count;                             // ��ʷ��������
    int history_index;                             // ��ǰ��ʷ��������
    int history_pos;                               // ��ʷ���λ��
    char current_dir[SHELL_MAX_PATH_LEN];          // ��ǰĿ¼
    bool esc_seq;                                  // ESC���б�־
    bool esc_bracket;                              // ESC [ ���б�־
    lfs_t *fs;                                     // �ļ�ϵͳʵ��ָ��
} shell_state_t;

// ��ʼ��Shell
void shell_init(lfs_t *fs);

// ������յ����ַ�
void shell_process(uint8_t *buffer, uint16_t len);

// ��ȡ��ǰShell״̬
shell_state_t* shell_get_state(void);

// ִ��Shell����(�ɹ��ⲿֱ�ӵ���)
int shell_execute(const char *cmd_line);

// ����ַ�����Shell����
void shell_print(const char *str);

// �����ʽ���ַ�����Shell����
void shell_printf(const char *fmt, ...);

void shell_set_uart(void *huart);

#endif /* SHELL_APP_H */ 