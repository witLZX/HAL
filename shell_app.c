#include "shell_app.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// Shell״̬ʵ��
static shell_state_t shell_state;

static void *uart_handle = NULL; // �û��ڳ�ʼ��ʱ����

// �������ǰ������
static int cmd_help(int argc, char *argv[]);
static int cmd_ls(int argc, char *argv[]);
static int cmd_cd(int argc, char *argv[]);
static int cmd_pwd(int argc, char *argv[]);
static int cmd_cat(int argc, char *argv[]);
static int cmd_mkdir(int argc, char *argv[]);
static int cmd_rm(int argc, char *argv[]);
static int cmd_touch(int argc, char *argv[]);
static int cmd_mv(int argc, char *argv[]);
static int cmd_cp(int argc, char *argv[]);
static int cmd_echo(int argc, char *argv[]);
static int cmd_clear(int argc, char *argv[]);
static int cmd_write(int argc, char *argv[]);

// ���������б�
static const shell_command_t commands[] = {
    {"help", "Display help information", cmd_help},
    {"ls", "List directory contents", cmd_ls},
    {"cd", "Change current directory", cmd_cd},
    {"pwd", "Print working directory", cmd_pwd},
    {"cat", "Display file contents", cmd_cat},
    {"mkdir", "Create directory", cmd_mkdir},
    {"rm", "Remove file or directory", cmd_rm},
    {"touch", "Create empty file or update timestamp", cmd_touch},
    {"mv", "Move/rename file", cmd_mv},
    {"cp", "Copy file", cmd_cp},
    {"echo", "Display text", cmd_echo},
    {"clear", "Clear screen", cmd_clear},
    {"write", "Write text to file", cmd_write},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

// Shell�ڲ����ܺ���ǰ������
static void shell_process_char(uint8_t ch);
static void shell_execute_line(void);
static void shell_handle_backspace(void);
static void shell_handle_tab(void);
static void shell_handle_escape_sequence(uint8_t ch);
static void shell_handle_history(int direction);
static void shell_add_to_history(const char *cmd);
static int shell_parse_args(char *cmd_line, char *argv[]);
static void shell_prompt(void);
static void shell_clear_line(void);
static char *shell_normalize_path(const char *path);

// ·����������
static char *shell_path_join(const char *dir, const char *file);
static int shell_is_dir(const char *path);

// ��ʼ��Shell
void shell_init(lfs_t *fs)
{
    memset(&shell_state, 0, sizeof(shell_state));
    shell_state.fs = fs;
    strcpy(shell_state.current_dir, "/");

    uart_handle = NULL; // �û�Ӧ���ⲿ����

    shell_printf("\r\n== LittleFS Shell v1.0 ==\r\n");
    shell_printf("Author: Microunion Studio\r\n");
    shell_prompt();
}

// ����UART���
void shell_set_uart(void *huart)
{
    uart_handle = huart;
}

// ��ȡShell״̬
shell_state_t *shell_get_state(void)
{
    return &shell_state;
}

// ������յ�������
void shell_process(uint8_t *buffer, uint16_t len)
{
    for (int i = 0; i < len; i++)
    {
        shell_process_char(buffer[i]);
    }
}

// �������ַ�
static void shell_process_char(uint8_t ch)
{
    // ����ESC���� (�������Ҽ���)
    if (shell_state.esc_seq)
    {
        if (shell_state.esc_bracket && ch == 'A')
        {
            // �ϼ� - ��һ����ʷ����
            shell_handle_history(-1);
        }
        else if (shell_state.esc_bracket && ch == 'B')
        {
            // �¼� - ��һ����ʷ����
            shell_handle_history(1);
        }
        else if (ch == '[')
        {
            shell_state.esc_bracket = true;
            return;
        }
        shell_state.esc_seq = false;
        shell_state.esc_bracket = false;
        return;
    }

    // ʶ��ESC���п�ʼ
    if (ch == 27)
    {
        shell_state.esc_seq = true;
        return;
    }

    // ����س�
    if (ch == '\r' || ch == '\n')
    {
        shell_printf("\r\n");
        if (shell_state.cmd_len > 0)
        {
            shell_execute_line();
        }
        shell_prompt();
        return;
    }

    // �����˸��
    if (ch == '\b' || ch == 127)
    {
        shell_handle_backspace();
        return;
    }

    // ����Tab�� (�����Զ���ȫ)
    if (ch == '\t')
    {
        shell_handle_tab();
        return;
    }

    // ������ͨ�ַ�
    if (ch >= 32 && ch < 127 && shell_state.cmd_len < SHELL_MAX_COMMAND_LENGTH - 1)
    {
        shell_state.cmd_buffer[shell_state.cmd_len++] = ch;
        shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
        shell_printf("%c", ch);
    }
}

// ִ��������
static void shell_execute_line(void)
{
    // ��¼��ʷ
    shell_add_to_history(shell_state.cmd_buffer);

    // ִ������
    shell_execute(shell_state.cmd_buffer);

    // ����������
    shell_state.cmd_buffer[0] = '\0';
    shell_state.cmd_len = 0;
    shell_state.history_pos = -1;
}

// ִ������
int shell_execute(const char *cmd_line)
{
    char cmd_copy[SHELL_MAX_COMMAND_LENGTH];
    char *argv[SHELL_MAX_ARGS];

    // �����������Ա����
    strncpy(cmd_copy, cmd_line, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    // ��������
    int argc = shell_parse_args(cmd_copy, argv);
    if (argc == 0)
    {
        return 0; // ������
    }

    // ���Ҳ�ִ������
    for (int i = 0; i < NUM_COMMANDS; i++)
    {
        if (strcmp(argv[0], commands[i].name) == 0)
        {
            return commands[i].function(argc, argv);
        }
    }

    // δ�ҵ�����
    shell_printf("Unknown command: %s\r\n", argv[0]);
    return -1;
}

// ���������в���
static int shell_parse_args(char *cmd_line, char *argv[])
{
    int argc = 0;
    char *token = strtok(cmd_line, " \t");

    while (token != NULL && argc < SHELL_MAX_ARGS)
    {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    return argc;
}

// �˸���
static void shell_handle_backspace(void)
{
    if (shell_state.cmd_len > 0)
    {
        shell_state.cmd_len--;
        shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
        shell_printf("\b \b"); // �˸񣬿ո����˸�
    }
}

// TAB���Զ���ȫ
static void shell_handle_tab(void)
{
    char *input = shell_state.cmd_buffer;
    int input_len = shell_state.cmd_len;
    
    // �������뿪ͷ�Ŀո�
    int cmd_start = 0;
    while (cmd_start < input_len && input[cmd_start] == ' ')
        cmd_start++;
    
    // ����������ȷ����ǰ��������������������ǲ���
    int cmd_end = cmd_start;
    while (cmd_end < input_len && input[cmd_end] != ' ')
        cmd_end++;
    
    int param_start = cmd_end;
    while (param_start < input_len && input[param_start] == ' ')
        param_start++;
    
    // ȷ����ǰ���������������
    char cmd_name[SHELL_MAX_COMMAND_LENGTH] = {0};
    if (cmd_end > cmd_start) {
        strncpy(cmd_name, input + cmd_start, cmd_end - cmd_start);
    }
    
    // ���1: �������������� - ʹ����������ȫ
    if (param_start >= input_len) {
        const char *prefix = input + cmd_start;
        int prefix_len = input_len - cmd_start;
        int matches = 0;
        int match_index = -1;
        
        for (int i = 0; i < NUM_COMMANDS; i++) {
            if (strncmp(prefix, commands[i].name, prefix_len) == 0) {
                matches++;
                match_index = i;
            }
        }
        
        if (matches == 1) {
            // ֻ��һ��ƥ�䣬ֱ�Ӳ�ȫ
            strcpy(shell_state.cmd_buffer, commands[match_index].name);
            shell_state.cmd_len = strlen(shell_state.cmd_buffer);
            
            // ��һ���ո��Ա��������
            shell_state.cmd_buffer[shell_state.cmd_len++] = ' ';
            shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
            
            // �����ǰ�в�������ʾ
            shell_clear_line();
            shell_printf("> %s", shell_state.cmd_buffer);
        } 
        else if (matches > 1) {
            // ���ƥ�䣬��ʾ���п��ܵ�����
            shell_printf("\r\nPossible commands:\r\n");
            for (int i = 0; i < NUM_COMMANDS; i++) {
                if (strncmp(prefix, commands[i].name, prefix_len) == 0) {
                    shell_printf("%s  ", commands[i].name);
                }
            }
            shell_printf("\r\n");
            shell_prompt();
            shell_printf("%s", shell_state.cmd_buffer);
        }
        return;
    }
    
    // ���2: ����������� - ���Բ�ȫ�ļ�/Ŀ¼��
    
    // ����Ƿ�Ϊ��Ҫ�ļ�·��������
    int requires_path = 0;
    int dir_only = 0; // ĳЩ����ֻ��ҪĿ¼����cd, mkdir
    
    if (strcmp(cmd_name, "cd") == 0 || strcmp(cmd_name, "mkdir") == 0) {
        requires_path = 1;
        dir_only = 1;
    } else if (strcmp(cmd_name, "cat") == 0 || strcmp(cmd_name, "rm") == 0 || 
               strcmp(cmd_name, "touch") == 0 || strcmp(cmd_name, "write") == 0 ||
               strcmp(cmd_name, "mv") == 0 || strcmp(cmd_name, "cp") == 0) {
        requires_path = 1;
    }
    
    if (!requires_path)
        return;
    
    // ������ǰ�����·������
    char partial_path[SHELL_MAX_PATH_LEN] = {0};
    strncpy(partial_path, input + param_start, input_len - param_start);
    
    // ����Ŀ¼���ļ�������
    char dir_part[SHELL_MAX_PATH_LEN] = {0};
    char file_part[SHELL_MAX_PATH_LEN] = {0};
    
    char *last_slash = strrchr(partial_path, '/');
    if (last_slash) {
        // ��Ŀ¼����
        strncpy(dir_part, partial_path, last_slash - partial_path + 1);
        strcpy(file_part, last_slash + 1);
    } else {
        // ֻ���ļ������֣�ʹ�õ�ǰĿ¼
        strcpy(dir_part, "./");
        strcpy(file_part, partial_path);
    }
    
    // �淶��Ŀ¼·��
    char *full_dir;
    if (dir_part[0] == 0) {
        full_dir = shell_normalize_path(".");
    } else {
        full_dir = shell_normalize_path(dir_part);
    }
    
    // �ڸ�Ŀ¼�в���ƥ����ļ���Ŀ¼
    lfs_dir_t dir;
    if (lfs_dir_open(shell_state.fs, &dir, full_dir) != LFS_ERR_OK) {
        lfs_dir_close(shell_state.fs, &dir);
        return; // �޷���Ŀ¼
    }
    
    // �ռ�ƥ����ļ���Ŀ¼
    struct {
        char name[LFS_NAME_MAX + 1];
        int is_dir;
    } matches[32];
    int match_count = 0;
    
    struct lfs_info info;
    int file_part_len = strlen(file_part);
    
    while (lfs_dir_read(shell_state.fs, &dir, &info) > 0 && match_count < 32) {
        // ���� . �� ..
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
            continue;
            
        // ������ƥ����ļ�/Ŀ¼
        if (strncmp(file_part, info.name, file_part_len) != 0)
            continue;
            
        // ���ֻ��ҪĿ¼���ⲻ��Ŀ¼������
        if (dir_only && info.type != LFS_TYPE_DIR)
            continue;
            
        // ��ӵ�ƥ���б�
        strncpy(matches[match_count].name, info.name, LFS_NAME_MAX);
        matches[match_count].name[LFS_NAME_MAX] = '\0';
        matches[match_count].is_dir = (info.type == LFS_TYPE_DIR);
        match_count++;
    }
    
    lfs_dir_close(shell_state.fs, &dir);
    
    if (match_count == 0) {
        return; // ��ƥ��
    } else if (match_count == 1) {
        // ֻ��һ��ƥ�䣬ֱ�Ӳ�ȫ
        
        // �ؽ�����������
        char new_cmd[SHELL_MAX_COMMAND_LENGTH] = {0};
        strncpy(new_cmd, input, param_start);
        
        // ���Ŀ¼����(�����)
        if (last_slash) {
            strncat(new_cmd, partial_path, last_slash - partial_path + 1);
        }
        
        // ���ƥ����ļ���
        strcat(new_cmd, matches[0].name);
        
        // �����Ŀ¼����Ӻ�׺/
        if (matches[0].is_dir) {
            strcat(new_cmd, "/");
        } else {
            // �ļ�����ӿո���ں�������
            strcat(new_cmd, " ");
        }
        
        // �����������
        strcpy(shell_state.cmd_buffer, new_cmd);
        shell_state.cmd_len = strlen(new_cmd);
        
        // �����ǰ�в�������ʾ
        shell_clear_line();
        shell_printf("> %s", shell_state.cmd_buffer);
    } else {
        // ���ƥ�䣬��ʾ���п��ܵ��ļ�/Ŀ¼
        shell_printf("\r\nPossible completions:\r\n");
        for (int i = 0; i < match_count; i++) {
            if (matches[i].is_dir) {
                shell_printf("[DIR]  %s/\r\n", matches[i].name);
            } else {
                shell_printf("[FILE] %s\r\n", matches[i].name);
            }
        }
        shell_printf("\r\n");
        shell_prompt();
        shell_printf("%s", shell_state.cmd_buffer);
    }
}

// ������ʷ���� (���¼�)
static void shell_handle_history(int direction)
{
    if (shell_state.history_count == 0)
    {
        return; // û����ʷ����
    }

    // ������ʷ����λ��
    if (shell_state.history_pos == -1)
    {
        // �״ΰ��ϼ������浱ǰ����
        shell_state.history_pos = 0;
    }
    else
    {
        shell_state.history_pos += direction;
    }

    // �߽���
    if (shell_state.history_pos < 0)
    {
        shell_state.history_pos = -1; // �ص���ǰ����
    }
    else if (shell_state.history_pos >= shell_state.history_count)
    {
        shell_state.history_pos = shell_state.history_count - 1;
    }

    // �����ǰ��
    shell_clear_line();

    if (shell_state.history_pos == -1)
    {
        // ʹ����ʱ���������
        shell_state.cmd_buffer[0] = '\0';
        shell_state.cmd_len = 0;
    }
    else
    {
        // ������ʷ����
        int idx = (shell_state.history_index - 1 - shell_state.history_pos + SHELL_HISTORY_SIZE) % SHELL_HISTORY_SIZE;
        strcpy(shell_state.cmd_buffer, shell_state.history[idx]);
        shell_state.cmd_len = strlen(shell_state.cmd_buffer);
    }

    // ��ʾ����
    shell_printf("> %s", shell_state.cmd_buffer);
}

// ��������ʷ��¼
static void shell_add_to_history(const char *cmd)
{
    if (strlen(cmd) == 0)
    {
        return; // �����������
    }

    // ����Ƿ�����һ��������ͬ
    if (shell_state.history_count > 0)
    {
        int last_idx = (shell_state.history_index - 1 + SHELL_HISTORY_SIZE) % SHELL_HISTORY_SIZE;
        if (strcmp(shell_state.history[last_idx], cmd) == 0)
        {
            return; // �������ظ�����
        }
    }

    // ����������
    strncpy(shell_state.history[shell_state.history_index], cmd, SHELL_MAX_COMMAND_LENGTH - 1);
    shell_state.history[shell_state.history_index][SHELL_MAX_COMMAND_LENGTH - 1] = '\0';

    shell_state.history_index = (shell_state.history_index + 1) % SHELL_HISTORY_SIZE;
    if (shell_state.history_count < SHELL_HISTORY_SIZE)
    {
        shell_state.history_count++;
    }
}

// ��ʾShell��ʾ��
static void shell_prompt(void)
{
    shell_printf("> ");
}

// �����ǰ��
static void shell_clear_line(void)
{
    shell_printf("\r\033[K"); // �ص����ײ��������
}

// �����ʽ���ַ���
void shell_printf(const char *fmt, ...)
{
    char buffer[128];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    shell_print(buffer);
}

// ����ַ���
void shell_print(const char *str)
{
    if (uart_handle)
    {
        my_printf(uart_handle, "%s", str);
    }
    else
    {
        // Ĭ��UARTδ���ã�����ʹ��ֱ�����
        printf("%s", str);
    }
}

// ·������ - �ϲ�Ŀ¼���ļ���
static char *shell_path_join(const char *dir, const char *file)
{
    static char full_path[SHELL_MAX_PATH_LEN];

    // ���file�Ǿ���·������ֱ��ʹ��
    if (file[0] == '/')
    {
        strncpy(full_path, file, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
        return full_path;
    }

    // ƴ��·��
    if (strcmp(dir, "/") == 0)
    {
        snprintf(full_path, sizeof(full_path), "/%s", file);
    }
    else
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, file);
    }

    return full_path;
}

// �淶��·�� (���� . �� .. ��)
static char *shell_normalize_path(const char *path)
{
    static char norm_path[SHELL_MAX_PATH_LEN];
    char *parts[SHELL_MAX_PATH_LEN / 2];
    int num_parts = 0;

    // ��������·���Ա㴦��
    char path_copy[SHELL_MAX_PATH_LEN];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // �������/���·��
    char *curr_path;
    if (path_copy[0] == '/')
    {
        // ����·��
        curr_path = path_copy + 1; // ������ͷ��'/'
        parts[num_parts++] = "/";
    }
    else
    {
        // ���·�����ȸ��Ƶ�ǰ·��
        curr_path = path_copy;
        if (strcmp(shell_state.current_dir, "/") == 0)
        {
            parts[num_parts++] = "/";
        }
        else
        {
            char curr_dir_copy[SHELL_MAX_PATH_LEN];
            strncpy(curr_dir_copy, shell_state.current_dir, sizeof(curr_dir_copy));

            // ����ǰĿ¼���Ϊ�������
            char *dir_part = strtok(curr_dir_copy, "/");
            while (dir_part && num_parts < SHELL_MAX_PATH_LEN / 2 - 1)
            {
                parts[num_parts++] = dir_part;
                dir_part = strtok(NULL, "/");
            }
        }
    }

    // ������·������
    char *part = strtok(curr_path, "/");
    while (part && num_parts < SHELL_MAX_PATH_LEN / 2 - 1)
    {
        if (strcmp(part, ".") == 0)
        {
            // ��ǰĿ¼������
        }
        else if (strcmp(part, "..") == 0)
        {
            // �ϼ�Ŀ¼������һ��
            if (num_parts > 1 || strcmp(parts[0], "/") != 0)
            {
                num_parts--;
            }
        }
        else
        {
            // ��ͨĿ¼��
            parts[num_parts++] = part;
        }
        part = strtok(NULL, "/");
    }

    // ���¹����淶��·��
    if (num_parts == 1 && strcmp(parts[0], "/") == 0)
    {
        strcpy(norm_path, "/");
    }
    else
    {
        norm_path[0] = '\0';
        for (int i = 0; i < num_parts; i++)
        {
            if (i == 0 && strcmp(parts[0], "/") == 0)
            {
                strcat(norm_path, "/");
            }
            else
            {
                strcat(norm_path, parts[i]);
                if (i < num_parts - 1)
                {
                    strcat(norm_path, "/");
                }
            }
        }
    }

    return norm_path;
}

// ���·���Ƿ���Ŀ¼
static int shell_is_dir(const char *path)
{
    lfs_dir_t dir;
    int res = lfs_dir_open(shell_state.fs, &dir, path);
    if (res == LFS_ERR_OK)
    {
        lfs_dir_close(shell_state.fs, &dir);
        return 1;
    }
    return 0;
}

/* ����ʵ�� */

// help����
static int cmd_help(int argc, char *argv[])
{
    shell_printf("Available commands:\r\n");
    for (int i = 0; i < NUM_COMMANDS; i++)
    {
        shell_printf("  %-8s - %s\r\n", commands[i].name, commands[i].help);
    }
    return 0;
}

// ls���� - ʹ����״��ʽ��ʾ
static int cmd_ls(int argc, char *argv[])
{
    const char *dir_path;
    int show_tree = 0; // Flag for tree display style

    // Process options
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tree") == 0)
        {
            show_tree = 1;
            // Shift arguments
            for (int j = i; j < argc - 1; j++)
            {
                argv[j] = argv[j + 1];
            }
            argc--;
            i--;
        }
    }

    // ȷ��Ŀ¼·��
    if (argc > 1)
    {
        dir_path = shell_normalize_path(argv[1]);
    }
    else
    {
        dir_path = shell_state.current_dir;
    }

    // ��Ŀ¼
    lfs_dir_t dir;
    int res = lfs_dir_open(shell_state.fs, &dir, dir_path);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot open directory '%s': Error %d\r\n", dir_path, res);
        return -1;
    }

    if (show_tree)
    {
        // Tree-style display
        shell_printf("Directory '%s' contents:\r\n", dir_path);

        // Read directory entries and store them to sort later
        struct
        {
            char name[LFS_NAME_MAX + 1];
            int is_dir;
            uint32_t size;
        } entries[64]; // Adjust size as needed

        int entry_count = 0;
        struct lfs_info info;

        while (true)
        {
            res = lfs_dir_read(shell_state.fs, &dir, &info);
            if (res <= 0)
                break; // End of directory or error

            // Skip . and ..
            if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
            {
                continue;
            }

            if (entry_count < 64)
            {
                strncpy(entries[entry_count].name, info.name, LFS_NAME_MAX);
                entries[entry_count].name[LFS_NAME_MAX] = '\0';
                entries[entry_count].is_dir = (info.type == LFS_TYPE_DIR);
                entries[entry_count].size = info.size;
                entry_count++;
            }
        }

        // Display sorted entries in tree style
        for (int i = 0; i < entry_count; i++)
        {
            if (entries[i].is_dir)
            {
                shell_printf("+-- [DIR] %s\r\n", entries[i].name);

                // Optionally list the directory contents if necessary
                if (argc > 1 && strcmp(argv[1], "-r") == 0)
                {
                    // Create full path for this directory
                    char subdir_path[SHELL_MAX_PATH_LEN];
                    if (strcmp(dir_path, "/") == 0)
                    {
                        snprintf(subdir_path, sizeof(subdir_path), "/%s", entries[i].name);
                    }
                    else
                    {
                        snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entries[i].name);
                    }

                    // Open the subdirectory
                    lfs_dir_t subdir;
                    int subdir_res = lfs_dir_open(shell_state.fs, &subdir, subdir_path);
                    if (subdir_res == LFS_ERR_OK)
                    {
                        // Read subdirectory entries
                        struct lfs_info subinfo;
                        while (true)
                        {
                            int subres = lfs_dir_read(shell_state.fs, &subdir, &subinfo);
                            if (subres <= 0)
                                break;

                            // Skip . and ..
                            if (strcmp(subinfo.name, ".") == 0 || strcmp(subinfo.name, "..") == 0)
                            {
                                continue;
                            }

                            // Print sub-entry with indent
                            if (subinfo.type == LFS_TYPE_DIR)
                            {
                                shell_printf("    +-- [DIR] %s\r\n", subinfo.name);
                            }
                            else
                            {
                                shell_printf("    +-- [FILE] %s (%lu bytes)\r\n",
                                             subinfo.name, (unsigned long)subinfo.size);
                            }
                        }

                        lfs_dir_close(shell_state.fs, &subdir);
                    }
                }
            }
            else
            {
                shell_printf("+-- [FILE] %s (%lu bytes)\r\n", entries[i].name, (unsigned long)entries[i].size);
            }
        }

        if (entry_count == 0)
        {
            shell_printf("    (empty directory)\r\n");
        }
    }
    else
    {
        // Traditional table display
        // ��ȡĿ¼��
        struct lfs_info info;
        shell_printf("Directory '%s' contents:\r\n", dir_path);
        shell_printf("  Type   Size     Name\r\n");
        shell_printf("  ----- -------- ------------------\r\n");

        while (true)
        {
            res = lfs_dir_read(shell_state.fs, &dir, &info);
            if (res < 0)
            {
                shell_printf("Failed to read directory: Error %d\r\n", res);
                break;
            }
            if (res == 0)
            {
                break; // ����Ŀ¼ĩβ
            }

            // ��ʾ���͡���С������
            const char *type_str = (info.type == LFS_TYPE_DIR) ? "[DIR] " : "[FILE]";
            shell_printf("  %-5s %8lu %s\r\n", type_str, info.size, info.name);
        }
    }

    lfs_dir_close(shell_state.fs, &dir);
    return 0;
}

// cd����
static int cmd_cd(int argc, char *argv[])
{
    if (argc < 2)
    {
        // ����������cd����ظ�Ŀ¼
        strcpy(shell_state.current_dir, "/");

        // After changing to root directory, list its contents
        shell_printf("Changed to root directory\r\n");

        // �����ʾĿ¼����
        lfs_dir_t dir;
        struct lfs_info info;
        if (lfs_dir_open(shell_state.fs, &dir, "/") == LFS_ERR_OK)
        {
            shell_printf("Contents:\r\n");

            // Count entries to determine if directory is empty
            int entry_count = 0;

            while (true)
            {
                int res = lfs_dir_read(shell_state.fs, &dir, &info);
                if (res <= 0)
                    break;

                if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
                    continue;

                entry_count++;

                if (info.type == LFS_TYPE_DIR)
                {
                    shell_printf("  [DIR]  %s\r\n", info.name);
                }
                else
                {
                    shell_printf("  [FILE] %s (%lu bytes)\r\n", info.name, (unsigned long)info.size);
                }
            }

            lfs_dir_close(shell_state.fs, &dir);

            if (entry_count == 0)
            {
                shell_printf("  (empty directory)\r\n");
            }
        }

        return 0;
    }

    // ��ȡ�淶��·��
    char *new_path = shell_normalize_path(argv[1]);

    // ���Ŀ¼�Ƿ����
    lfs_dir_t dir;
    int res = lfs_dir_open(shell_state.fs, &dir, new_path);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot change to directory '%s': Error %d\r\n", new_path, res);
        return -1;
    }

    // Ŀ¼���ڣ����µ�ǰĿ¼
    strcpy(shell_state.current_dir, new_path);

    // After changing directory, display its contents in a simple format
    shell_printf("Changed to '%s'\r\n", new_path);
    shell_printf("Contents:\r\n");

    // Count entries to determine if directory is empty
    int entry_count = 0;

    // Read directory entries
    struct lfs_info info;
    while (true)
    {
        res = lfs_dir_read(shell_state.fs, &dir, &info);
        if (res <= 0)
            break;

        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
            continue;

        entry_count++;

        if (info.type == LFS_TYPE_DIR)
        {
            shell_printf("  [DIR]  %s\r\n", info.name);
        }
        else
        {
            shell_printf("  [FILE] %s (%lu bytes)\r\n", info.name, (unsigned long)info.size);
        }
    }

    lfs_dir_close(shell_state.fs, &dir);

    if (entry_count == 0)
    {
        shell_printf("  (empty directory)\r\n");
    }

    return 0;
}

// pwd����
static int cmd_pwd(int argc, char *argv[])
{
    shell_printf("%s\r\n", shell_state.current_dir);
    return 0;
}

// cat����
static int cmd_cat(int argc, char *argv[])
{
    if (argc < 2)
    {
        shell_printf("Usage: cat <filename>\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *file_path = shell_normalize_path(argv[1]);

    // ���ļ�
    lfs_file_t file;
    int res = lfs_file_open(shell_state.fs, &file, file_path, LFS_O_RDONLY);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot open file '%s': Error %d\r\n", file_path, res);
        return -1;
    }

    // ��ȡ����ʾ�ļ�����
    char buffer[SHELL_MAX_LINE_LEN + 1];
    lfs_ssize_t bytes_read;

    while ((bytes_read = lfs_file_read(shell_state.fs, &file, buffer, SHELL_MAX_LINE_LEN)) > 0)
    {
        buffer[bytes_read] = '\0';
        shell_printf("%s", buffer);
    }

    if (bytes_read < 0)
    {
        shell_printf("\r\nFile read failed: Error %d\r\n", (int)bytes_read);
    }
    else
    {
        shell_printf("\r\n");
    }

    lfs_file_close(shell_state.fs, &file);
    return (bytes_read >= 0) ? 0 : -1;
}

// mkdir����
static int cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2)
    {
        shell_printf("Usage: mkdir <directory>\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *dir_path = shell_normalize_path(argv[1]);

    // ����Ŀ¼
    int res = lfs_mkdir(shell_state.fs, dir_path);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot create directory '%s': Error %d\r\n", dir_path, res);
        return -1;
    }

    shell_printf("Directory '%s' created successfully\r\n", dir_path);
    return 0;
}

// rm����
static int cmd_rm(int argc, char *argv[])
{
    if (argc < 2)
    {
        shell_printf("Usage: rm <filename>\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *path = shell_normalize_path(argv[1]);

    // ����Ƿ���Ŀ¼
    if (shell_is_dir(path))
    {
        // ����ɾ��Ŀ¼
        int res = lfs_remove(shell_state.fs, path);
        if (res != LFS_ERR_OK)
        {
            shell_printf("Cannot remove directory '%s': Error %d\r\n", path, res);
            shell_printf("Note: Directory may not be empty\r\n");
            return -1;
        }
    }
    else
    {
        // ɾ���ļ�
        int res = lfs_remove(shell_state.fs, path);
        if (res != LFS_ERR_OK)
        {
            shell_printf("Cannot remove file '%s': Error %d\r\n", path, res);
            return -1;
        }
    }

    shell_printf("'%s' removed\r\n", path);
    return 0;
}

// touch����
static int cmd_touch(int argc, char *argv[])
{
    if (argc < 2)
    {
        shell_printf("Usage: touch <filename>\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *file_path = shell_normalize_path(argv[1]);

    // ���Դ��ļ����������򴴽�
    lfs_file_t file;
    int res = lfs_file_open(shell_state.fs, &file, file_path, LFS_O_RDWR | LFS_O_CREAT);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot create file '%s': Error %d\r\n", file_path, res);
        return -1;
    }

    // �ر��ļ�
    lfs_file_close(shell_state.fs, &file);
    shell_printf("File '%s' created/updated\r\n", file_path);
    return 0;
}

// mv����
static int cmd_mv(int argc, char *argv[])
{
    if (argc < 3)
    {
        shell_printf("Usage: mv <source> <destination>\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *src_path = shell_normalize_path(argv[1]);
    char *dst_path = shell_normalize_path(argv[2]);

    // ִ��������/�ƶ�
    int res = lfs_rename(shell_state.fs, src_path, dst_path);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot move/rename '%s' to '%s': Error %d\r\n", src_path, dst_path, res);
        return -1;
    }

    shell_printf("'%s' moved/renamed to '%s'\r\n", src_path, dst_path);
    return 0;
}

// cp����
static int cmd_cp(int argc, char *argv[])
{
    if (argc < 3)
    {
        shell_printf("Usage: cp <source> <destination>\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *src_path = shell_normalize_path(argv[1]);
    char *dst_path = shell_normalize_path(argv[2]);

    // ��Դ�ļ�
    lfs_file_t src_file;
    int res = lfs_file_open(shell_state.fs, &src_file, src_path, LFS_O_RDONLY);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot open source file '%s': Error %d\r\n", src_path, res);
        return -1;
    }

    // ����/����Ŀ���ļ�
    lfs_file_t dst_file;
    res = lfs_file_open(shell_state.fs, &dst_file, dst_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot create destination file '%s': Error %d\r\n", dst_path, res);
        lfs_file_close(shell_state.fs, &src_file);
        return -1;
    }

    // ��������
    char buffer[128];
    lfs_ssize_t bytes_read, bytes_written;
    lfs_ssize_t total_bytes = 0;

    while ((bytes_read = lfs_file_read(shell_state.fs, &src_file, buffer, sizeof(buffer))) > 0)
    {
        bytes_written = lfs_file_write(shell_state.fs, &dst_file, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            shell_printf("Error writing to destination file: Requested %ld bytes, wrote %ld bytes\r\n",
                         (long)bytes_read, (long)bytes_written);
            lfs_file_close(shell_state.fs, &src_file);
            lfs_file_close(shell_state.fs, &dst_file);
            return -1;
        }
        total_bytes += bytes_written;
    }

    // �ر��ļ�
    lfs_file_close(shell_state.fs, &src_file);
    lfs_file_close(shell_state.fs, &dst_file);

    if (bytes_read < 0)
    {
        shell_printf("Error reading source file: %d\r\n", (int)bytes_read);
        return -1;
    }

    shell_printf("Copied '%s' to '%s', %ld bytes total\r\n", src_path, dst_path, (long)total_bytes);
    return 0;
}

// echo����
static int cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        shell_printf("%s", argv[i]);
        if (i < argc - 1)
        {
            shell_printf(" ");
        }
    }
    shell_printf("\r\n");
    return 0;
}

// clear����
static int cmd_clear(int argc, char *argv[])
{
    shell_printf("\033[2J\033[H"); // ������������������Ͻ�
    return 0;
}

// write���� - ���ļ�д���ı�����
static int cmd_write(int argc, char *argv[])
{
    if (argc < 3)
    {
        shell_printf("Usage: write <filename> <text>\r\n");
        shell_printf("Example: write test.txt Hello world\r\n");
        return -1;
    }

    // ��ȡ�淶��·��
    char *file_path = shell_normalize_path(argv[1]);

    // ���ļ�����д��(������������)
    lfs_file_t file;
    int res = lfs_file_open(shell_state.fs, &file, file_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (res != LFS_ERR_OK)
    {
        shell_printf("Cannot open file '%s' for writing: Error %d\r\n", file_path, res);
        return -1;
    }

    // �������е��ı�д���ļ�
    for (int i = 2; i < argc; i++)
    {
        // д�����
        lfs_ssize_t w_sz = lfs_file_write(shell_state.fs, &file, argv[i], strlen(argv[i]));
        if (w_sz < 0)
        {
            shell_printf("Failed to write to file: Error %d\r\n", (int)w_sz);
            lfs_file_close(shell_state.fs, &file);
            return -1;
        }

        // ����������һ����������ӿո�
        if (i < argc - 1)
        {
            lfs_file_write(shell_state.fs, &file, " ", 1);
        }
    }

    lfs_file_close(shell_state.fs, &file);
    shell_printf("Data written to '%s'\r\n", file_path);
    return 0;
}