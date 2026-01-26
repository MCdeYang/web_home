/* photos.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include "photos.h"



// =============== 安全校验 ===============
int photos_is_safe_filename(const char *name) {
    if (!name || strlen(name) == 0 || strlen(name) > 250) {
        return 0;
    }
    // 禁止以 . 开头（隐藏文件）
    if (name[0] == '.') return 0;

    // 禁止路径遍历和危险字符
    const char *p = name;
    while (*p) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            return 0;
        }
        p++;
    }

    // 检查扩展名（不区分大小写）
    const char *ext = strrchr(name, '.');
    if (!ext || ext == name) return 0; // 必须有扩展名，且不是 ".jpg"
    ext++;

    return (strcasecmp(ext, "jpg") == 0 ||
            strcasecmp(ext, "jpeg") == 0 ||
            strcasecmp(ext, "png") == 0 ||
            strcasecmp(ext, "gif") == 0);
}

// =============== 响应输出 ===============
void photos_send_json_response(const char *json_str) {
    printf("Content-Type: application/json\r\n");
    printf("Content-Length: %zu\r\n", strlen(json_str));
    printf("\r\n");
    printf("%s", json_str);
    fflush(stdout);
}

// =============== 列表生成 ===============
char* photos_build_list_json(void) {
    DIR *dir = opendir(PHOTOS_DIR);
    if (!dir) {
        char *err = malloc(64);
        strcpy(err, "{\"error\":\"open dir failed\"}");
        return err;
    }

    char *json = malloc(8192);
    if (!json) {
        closedir(dir);
        char *err = malloc(64);
        strcpy(err, "{\"error\":\"malloc failed\"}");
        return err;
    }

    strcpy(json, "[");
    int first = 1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && photos_is_safe_filename(entry->d_name)) {
            if (!first) strcat(json, ",");
            char escaped[260];
            snprintf(escaped, sizeof(escaped), "\"%s\"", entry->d_name);
            strcat(json, escaped);
            first = 0;
        }
    }
    closedir(dir);
    strcat(json, "]");

    return json;
}

// =============== 文件保存（multipart 解析） ===============
char* photos_save_uploaded_file(const char *body, const char *content_type) {
    if (!body || !content_type) return NULL;

    // === 1. 正确提取 boundary（支持带引号和空格）===
    const char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start) return NULL;
    boundary_start += 9; // skip "boundary="

    while (*boundary_start == ' ') boundary_start++;

    char raw_boundary[256] = {0};
    const char *bnd_end = NULL;

    if (*boundary_start == '"') {
        boundary_start++;
        bnd_end = strchr(boundary_start, '"');
    } else {
        bnd_end = strchr(boundary_start, ';');
        if (!bnd_end) bnd_end = boundary_start + strlen(boundary_start);
    }

    if (!bnd_end || (size_t)(bnd_end - boundary_start) >= sizeof(raw_boundary) - 1) {
        return NULL;
    }
    memcpy(raw_boundary, boundary_start, bnd_end - boundary_start);
    raw_boundary[bnd_end - boundary_start] = '\0';

    // === 2. 查找 filename="..." ===
    const char *file_start = strstr(body, "filename=\"");
    if (!file_start) return NULL;

    const char *fname_start = file_start + 10;
    const char *fname_end = strchr(fname_start, '"');
    if (!fname_end) return NULL;

    size_t fname_len = fname_end - fname_start;
    if (fname_len == 0 || fname_len > 250) return NULL;

    char *filename = malloc(fname_len + 1);
    if (!filename) return NULL;
    memcpy(filename, fname_start, fname_len);
    filename[fname_len] = '\0';

    // === 3. 安全检查 ===
    if (!photos_is_safe_filename(filename)) {
        free(filename);
        return NULL;
    }

    // === 4. 定位数据起始 ===
    const char *data_start = strstr(fname_end, "\r\n\r\n");
    if (!data_start) {
        free(filename);
        return NULL;
    }
    data_start += 4;

    // === 5. 精确查找数据结束位置 ===
    const char *data_end = NULL;
    size_t body_len = strlen(body); // 注意：依赖 body 以 \0 结尾

    // 尝试标准结束标记: \r\n--{boundary}--\r\n
    char end_marker1[300];
    int len1 = snprintf(end_marker1, sizeof(end_marker1), "\r\n--%s--\r\n", raw_boundary);
    if (len1 > 0 && (size_t)len1 < sizeof(end_marker1)) {
        data_end = (const char*)memmem(data_start, body + body_len - data_start,
                                       end_marker1, len1);
    }

    // 尝试无尾部 \r\n 的结束（某些客户端）
    if (!data_end) {
        char end_marker2[300];
        int len2 = snprintf(end_marker2, sizeof(end_marker2), "\r\n--%s--", raw_boundary);
        if (len2 > 0 && (size_t)len2 < sizeof(end_marker2)) {
            data_end = (const char*)memmem(data_start, body + body_len - data_start,
                                           end_marker2, len2);
        }
    }

    // 尝试 part 分隔符（多文件上传）
    if (!data_end) {
        char mid_marker[300];
        int len_mid = snprintf(mid_marker, sizeof(mid_marker), "\r\n--%s\r\n", raw_boundary);
        if (len_mid > 0 && (size_t)len_mid < sizeof(mid_marker)) {
            data_end = (const char*)memmem(data_start, body + body_len - data_start,
                                           mid_marker, len_mid);
        }
    }

    // 最终 fallback：用整个 body 剩余部分
    if (!data_end) {
        data_end = body + body_len;
    }

    size_t file_size = data_end - data_start;
    if (file_size == 0) {
        free(filename);
        return NULL;
    }

    // === 6. 保存文件 ===
    char filepath[512];
    if ((size_t)snprintf(filepath, sizeof(filepath), "%s/%s", PHOTOS_DIR, filename) >= sizeof(filepath)) {
        free(filename);
        return NULL;
    }

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        free(filename);
        return NULL;
    }
    fwrite(data_start, 1, file_size, fp);
    fclose(fp);

    return filename; // caller must free
}
char* photos_extract_filename_from_json(const char *json_body) {
    if (!json_body) return NULL;

    json_object *root = json_tokener_parse(json_body);
    if (!root) {
        return NULL; // 解析失败
    }

    json_object *fname_obj;
    // 安全获取 "filename" 字段
    if (!json_object_object_get_ex(root, "filename", &fname_obj)) {
        json_object_put(root);
        return NULL;
    }

    // 必须是字符串类型
    if (json_object_get_type(fname_obj) != json_type_string) {
        json_object_put(root);
        return NULL;
    }

    const char *fname_str = json_object_get_string(fname_obj);
    if (!fname_str || strlen(fname_str) == 0) {
        json_object_put(root);
        return NULL;
    }

    // 复制字符串（调用者负责 free）
    char *result = strdup(fname_str);
    json_object_put(root); // 释放整个 JSON 对象
    return result;
}

// ===== 安全删除文件 =====
int photos_delete_photo_file(const char *filename) {
    if (!filename) return 0;
    if (!photos_is_safe_filename(filename)) return 0;

    char filepath[512];
    // ✅ 使用统一的 PHOTOS_DIR 宏
    snprintf(filepath, sizeof(filepath), "%s/%s", PHOTOS_DIR, filename);

    return (unlink(filepath) == 0) ? 1 : 0;
}
static unsigned char hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0; // invalid, but we assume input is valid
}
void photo_url_decode(const char *src, char *dst) {
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            unsigned char high = hex_char_to_byte(src[1]);
            unsigned char low  = hex_char_to_byte(src[2]);
            *dst++ = (high << 4) | low;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}