#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "error.h"


void json_success(const char *msg) {
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "message", json_object_new_string(msg));
    printf("Content-Type: application/json\r\n\r\n");
    printf("%s\n", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
    json_object_put(obj);
}
// 内部通用函数：发送带状态行和 JSON body 的错误响应
void _send_json_error(int status_code, const char *status_text, const char *message) {
    // 发送 Status 行
    if (status_code != 200) {
        fprintf(stdout, "Status: %d %s\r\n", status_code, status_text);
    }
    // 发送 Content-Type 头
    fputs("Content-Type: application/json\r\n\r\n", stdout);
    // 构建 JSON 对象
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "error", json_object_new_string(message));
    json_object_object_add(obj, "code", json_object_new_int(status_code));
    // 输出 JSON 字符串
    const char *json_str = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    fputs(json_str, stdout);
    fputc('\n', stdout);

    json_object_put(obj);

    fflush(stdout);
}

// 公共错误函数
void send_error_400(const char *message) {
    _send_json_error(400, "Bad Request", message ? message : "Invalid request");
}

void send_error_401(const char *message) {
    _send_json_error(401, "Unauthorized", message ? message : "Authentication required");
}

void send_error_403(const char *message) {
    _send_json_error(403, "Forbidden", message ? message : "Access denied");
}

void send_error_404(const char *message) {
    _send_json_error(404, "Not Found", message ? message : "Endpoint not found");
}

void send_error_405(const char *message) {
    _send_json_error(405, "Method Not Allowed", message ? message : "HTTP method not supported");
}

void send_error_500(const char *message) {
    _send_json_error(500, "Internal Server Error", message ? message : "Internal error occurred");
}