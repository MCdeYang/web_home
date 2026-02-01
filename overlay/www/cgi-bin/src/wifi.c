#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "wifi.h"



int run_command_capture(const char *cmd, char *output, size_t out_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    output[0] = '\0';
    if (fgets(output, (int)out_size, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    char *nl = strchr(output, '\n');
    if (nl) *nl = '\0';
    pclose(fp);
    return 0;
}

// 判断字符串是否只包含安全字符（字母、数字、-_. 和空格）
int is_safe_string(const char *str, int allow_space) {
    for (const char *p = str; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '_' || *p == '-' || *p == '.' || *p == '——' || *p == '@') {
            continue;
        }
        if (allow_space && *p == ' ') {
            continue;
        }
        return 0; // unsafe
    }
    return 1; // safe
}
