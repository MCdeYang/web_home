#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <json-c/json.h>
#include "settings.h"


// ----------------------------
// 工具函数
// ----------------------------

// 安全地从 "key:value" 行中提取 value（假设无空格）
void extract_value(const char *line, const char *key, char *out, size_t out_size) {
    size_t key_len = strlen(key);
    if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
        const char *val = line + key_len + 1;
        size_t val_len = strcspn(val, "\r\n");
        if (val_len >= out_size) val_len = out_size - 1;
        strncpy(out, val, val_len);
        out[val_len] = '\0';
    }
}

// 简易 JSON 转义（仅处理双引号和反斜杠）
void json_escape(const char *src, char *dst, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j + 2 < dst_size) {
        if (src[i] == '"' || src[i] == '\\') {
            dst[j++] = '\\';
        }
        dst[j++] = src[i++];
    }
    dst[j] = '\0';
}

// 写 login.txt
int write_login_file(const char *username, const char *password) {
    FILE *f = fopen(LOGIN_FILE, "w");
    if (!f) return -1;
    fprintf(f, "username:%s\npassword:%s\n", username, password);
    fclose(f);
    return 0;
}

// 从 login.txt 读取用户名（密码不返回）
void read_username(char *username, size_t size) {
    FILE *f = fopen(LOGIN_FILE, "r");
    if (!f) {
        strncpy(username, "admin", size - 1);
        username[size - 1] = '\0';
        return;
    }
    char line[256];
    if (fgets(line, sizeof(line), f)) {
        extract_value(line, "username", username, size);
    } else {
        strncpy(username, "admin", size - 1);
    }
    fclose(f);
    if (username[0] == '\0') strncpy(username, "admin", size - 1);
}

// 从 public_enabled.txt 读取状态
int is_ngrok_running() {
    const char *pid_file = "/development/web_tunnel/ngrok.pid";
    FILE *f = fopen(pid_file, "r");
    if (!f) return 0; // PID 文件不存在 → 未运行

    pid_t pid;
    if (fscanf(f, "%d", &pid) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);

    // kill -0 检查进程是否存在（不发送信号）
    if (kill(pid, 0) == 0) {
        return 1; // 进程存在
    } else {
        // 进程已死，清理残留 PID 文件（可选）
        unlink(pid_file);
        return 0;
    }
}

// 简易 JSON 解析：提取 "username" 和 "password"
void parse_json_credentials(const char *body, char *username, char *password, size_t max_len) {
    username[0] = '\0';
    password[0] = '\0';

    if (!body) return;

    json_object *obj = json_tokener_parse(body);
    if (!obj || !json_object_is_type(obj, json_type_object)) {
        json_object_put(obj);
        return;
    }

    json_object *juser, *jpass;
    if (json_object_object_get_ex(obj, "username", &juser)) {
        const char *u = json_object_get_string(juser);
        if (u) {
            strncpy(username, u, max_len - 1);
            username[max_len - 1] = '\0';
        }
    }
    if (json_object_object_get_ex(obj, "password", &jpass)) {
        const char *p = json_object_get_string(jpass);
        if (p) {
            strncpy(password, p, max_len - 1);
            password[max_len - 1] = '\0';
        }
    }

    json_object_put(obj);
}
/*
void send_json_response(int status_code, const char *status_msg, const char *json) {
    printf("Status: %d %s\r\n", status_code, status_msg);
    printf("Content-Type: application/json\r\n");
    printf("\r\n");
    printf("%s", json);
    fflush(stdout);
}
*/
int load_stored_credentials(char *stored_user, size_t user_size,
                                   char *stored_pass, size_t pass_size) {
    FILE *f = fopen(LOGIN_FILE, "r");
    if (!f) return -1;

    char line[256];
    stored_user[0] = '\0';
    stored_pass[0] = '\0';

    while (fgets(line, sizeof(line), f)) {
        // 移除换行符
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "username:", 9) == 0) {
            strncpy(stored_user, line + 9, user_size - 1);
            stored_user[user_size - 1] = '\0';
        } else if (strncmp(line, "password:", 9) == 0) {
            strncpy(stored_pass, line + 9, pass_size - 1);
            stored_pass[pass_size - 1] = '\0';
        }
    }

    fclose(f);

    // 确保两个字段都读到了
    return (stored_user[0] && stored_pass[0]) ? 0 : -1;
}