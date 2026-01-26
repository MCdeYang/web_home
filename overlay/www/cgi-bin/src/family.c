#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <json-c/json.h>
#include "family.h"



// ———————— 工具函数 ————————

// 从 Cookie 中提取 user=xxx（不验证）
char* get_username_from_cookie() {
    const char *cookie = getenv("HTTP_COOKIE");
    if (!cookie) return NULL;

    char *p = strstr(cookie, "user=");
    if (!p) return NULL;
    p += 5; // skip "user="

    char *end = strchr(p, ';');
    int len = end ? (end - p) : strlen(p);
    if (len <= 0 || len > 64) return NULL;

    char *user = malloc(len + 1);
    if (!user) return NULL;
    strncpy(user, p, len);
    user[len] = '\0';

    // 去首尾空格
    while (*user == ' ') memmove(user, user+1, strlen(user));
    int last = strlen(user) - 1;
    while (last >= 0 && user[last] == ' ') user[last--] = '\0';

    return user;
}

// 安全加载 JSON，自动初始化空结构
json_object* load_family_data() {
    FILE *fp = fopen(FAMILY_DATA_PATH, "r");
    if (!fp) {
        json_object *root = json_object_new_object();
        json_object_object_add(root, "members", json_object_new_array());
        return root;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size == 0) {
        fclose(fp);
        json_object *root = json_object_new_object();
        json_object_object_add(root, "members", json_object_new_array());
        return root;
    }

    char *buf = malloc(size + 1);
    if (!buf) { fclose(fp); return NULL; }
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);

    json_object *root = json_tokener_parse(buf);
    free(buf);

    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        root = json_object_new_object();
        json_object_object_add(root, "members", json_object_new_array());
    }

    json_object *members = json_object_object_get(root, "members");
    if (!members || !json_object_is_type(members, json_type_array)) {
        json_object_object_del(root, "members");
        json_object_object_add(root, "members", json_object_new_array());
    }

    return root;
}

// 保存 JSON 文件
int save_family_data(json_object *root) {
    FILE *fp = fopen(FAMILY_DATA_PATH, "w");
    if (!fp) return -1;
    const char *str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    int ok = (fprintf(fp, "%s\n", str) > 0);
    fclose(fp);
    return ok ? 0 : -1;
}

time_t parse_due_date(const char *due_str) {
    if (!due_str) return -1;
    struct tm tm = {0};
    int y, m, d, h, min;
    if (sscanf(due_str, "%d-%d-%d %d:%d", &y, &m, &d, &h, &min) == 5) {
        tm.tm_year = y - 1900;
        tm.tm_mon  = m - 1;
        tm.tm_mday = d;
        tm.tm_hour = h;
        tm.tm_min  = min;
        tm.tm_isdst = -1;
        return mktime(&tm);
    }
    return -1;
}