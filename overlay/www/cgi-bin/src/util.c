#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <json-c/json.h>
#include "util.h"

#define MAX_FILENAME 512
#define MAX_PATH     1024

int parse_multipart_and_save(
    const char *body,
    long content_length,
    const char *boundary,
    const char *target_dir
) {
    if (!body || !boundary || !target_dir) return -1;

    char full_boundary[300];
    snprintf(full_boundary, sizeof(full_boundary), "--%s", boundary);

    // 遍历每一个 part
    const char *current = body;
    while ((current = strstr(current, full_boundary)) != NULL) {
        current += strlen(full_boundary); // 跳过 "--boundary"

        // 检查是否是结束边界 "--boundary--"
        if (strncmp(current, "--", 2) == 0) {
            break;
        }

        // 查找头部结束位置 (\r\n\r\n)
        const char *headers_end = strstr(current, "\r\n\r\n");
        if (!headers_end) {
            current += 1; // 继续搜索
            continue;
        }

        // 检查该 part 是否包含 filename（即是否为文件）
        int is_file_part = 0;
        const char *line_start = current;
        char filename[MAX_FILENAME] = "";

        while (line_start < headers_end) {
            const char *line_end = strstr(line_start, "\r\n");
            if (!line_end || line_end > headers_end) break;

            size_t len = line_end - line_start;
            if (len >= sizeof(filename)) len = sizeof(filename) - 1;

            char header_line[1024];
            strncpy(header_line, line_start, len);
            header_line[len] = '\0';

            if (strncasecmp(header_line, "Content-Disposition:", 20) == 0) {
                if (strstr(header_line, "filename=") != NULL) {
                    if (extract_filename_from_header(header_line, filename, sizeof(filename))) {
                        if (!is_safe_filename(filename)) {
                            return -2; // unsafe filename
                        }
                        is_file_part = 1;
                    }
                    break;
                }
            }
            line_start = line_end + 2;
        }

        if (!is_file_part) {
            current = headers_end + 4;
            continue;
        }

        // 找到文件 part，开始保存
        const char *data_start = headers_end + 4;
        const char *next_boundary = strstr(data_start, full_boundary);
        if (!next_boundary) {
            next_boundary = body + content_length;
        }

        size_t data_len = next_boundary - data_start;
        if (data_len == 0) {
            return -1;
        }

        // 构建完整路径
        char final_path[MAX_PATH];
        snprintf(final_path, sizeof(final_path), "%s/%s", target_dir, filename);

        // 确保目标目录存在且可写
        struct stat st;
        if (stat(target_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return -3; // 目录不存在或不是目录
        }
        // 可选：确保目录可写（调试用）
        // chmod(target_dir, 0777);

        FILE *fp = fopen(final_path, "wb");
        if (!fp) {
            return -3;
        }

        if (fwrite(data_start, 1, data_len, fp) != data_len) {
            fclose(fp);
            unlink(final_path);
            return -3;
        }
        fclose(fp);
        return 0; // 成功
    }

    return -1; // 未找到任何文件 part
}
/*
int safe_filename(const char *name) {
    if (!name || name[0] == '\0' || strchr(name, '/') || strchr(name, '\\'))
        return 0;
    // 不允许以 . 开头（防 .htaccess 等）
    if (name[0] == '.') return 0;
    // 不允许包含 ..
    if (strstr(name, "..")) return 0;
    // 只允许字母、数字、点、下划线、连字符、空格（可选）
    for (const char *p = name; *p; p++) {
        if (isalnum(*p) || *p == '.' || *p == '_' || *p == '-' || *p == ' ')
            continue;
        return 0;
    }
    return 1;
}
*/
int make_parent_writable(const char *dirpath) {
    struct stat st;
    if (stat(dirpath, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            return -1; // 不是目录
        }
        // 如果当前权限不是 777，尝试修复
        if ((st.st_mode & 0777) != 0777) {
            if (chmod(dirpath, 0777) != 0) {
                return -1;
            }
        }
        return 0;
    }
    return -1; // 不存在或错误
}
int mkpath(const char *dir, mode_t mode) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    // 防止缓冲区溢出
    if (strlen(dir) >= sizeof(tmp)) return -1;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);

    // 去掉末尾的 '/'
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }

    // 从第一个 '/' 后开始逐级创建
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                *p = '/';
                return -1;
            }
            *p = '/';
        }
    }

    // 创建最后一级
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}
int extract_filename_from_header(const char *header, char *out_filename, size_t out_size) {
    if (!header || !out_filename || out_size == 0) {
        return 0;
    }
    out_filename[0] = '\0';

    // 优先尝试 filename*=UTF-8''...
    const char *p = strstr(header, "filename*=");
    if (p) {
        p += 10; // skip "filename*="
        while (*p == ' ') p++;
        if (strncmp(p, "UTF-8''", 7) == 0) {
            p += 7;
            char decoded[512];
            url_decode(p, decoded);
            if (decoded[0] != '\0') {
                strncpy(out_filename, decoded, out_size - 1);
                out_filename[out_size - 1] = '\0';
                return 1;
            }
        }
    }

    // 回退到 filename="..."
    p = strstr(header, "filename=");
    if (p) {
        p += 9;
        while (*p == ' ') p++;

        const char *start = p;
        const char *end = NULL;

        if (*p == '"') {
            start = p + 1;
            // 查找未转义的 "
            end = start;
            while (*end) {
                if (*end == '"' && *(end - 1) != '\\') {
                    break;
                }
                end++;
            }
            if (*end != '"') end = NULL; // 未找到闭合引号
        } else {
            // 无引号：取到行尾或 ; 或 \r
            end = strchr(start, ';');
            if (!end) end = strchr(start, '\r');
            if (!end) end = start + strlen(start);
        }

        if (end && end > start) {
            size_t len = end - start;
            if (len >= out_size) len = out_size - 1;
            // 复制并解码（虽然通常不需要，但安全起见）
            char temp[512];
            strncpy(temp, start, len);
            temp[len] = '\0';
            url_decode(temp, out_filename); // 有些浏览器会编码非ASCII
            return 1;
        }
    }

    return 0;
}

int is_safe_filename(const char *name) {
    if (!name || !*name) return 0;
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
        return 0;
    }
    return 1;
}

int is_safe_relative_path(const char *path) {
    if (!path || !*path) return 0;
    if (strstr(path, "..") || strstr(path, "//")) {
        return 0;
    }
    return 1;
}
static const char* extract_value(const char *json, const char *key, char *out, size_t out_size) {
    char key_str[256];
    snprintf(key_str, sizeof(key_str), "\"%s\":", key);

    const char *p = strstr(json, key_str);
    if (!p) return NULL;

    p += strlen(key_str);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    if (*p != '"') return NULL;
    p++;

    const char *end = strchr(p, '"');
    if (!end || (size_t)(end - p) >= out_size) return NULL;

    memcpy(out, p, end - p);
    out[end - p] = '\0';
    return out;
}

int parse_rename_json(const char *body, char *path, char *old_name, char *new_name) {
    return 
        extract_value(body, "path", path, 1024) &&
        extract_value(body, "old_name", old_name, 512) &&
        extract_value(body, "new_name", new_name, 512);
}
static void url_decode_inplace(char *str) {
    if (!str) return;
    char *src = str;
    char *dst = str;
    while (*src) {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = { src[1], src[2], '\0' };
            *dst = (char)strtol(hex, NULL, 16);
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

/**
 * @brief 从 query string 中提取指定 key 的值，并自动 URL 解码
 * @param query: 原始查询字符串（如 "name=a.jpg&note=hello%20world"），不能为 NULL
 * @param key: 要查找的键（如 "name"），不能为 NULL
 * @param out: 输出缓冲区，不能为 NULL
 * @param out_len: 缓冲区大小（必须 > 0）
 * @return 0 成功，-1 失败（未找到或参数无效）
 */
int parse_query_string(const char *query, const char *key, char *out, size_t out_len) {
    if (!query || !key || !out || out_len == 0) {
        return -1;
    }

    // 计算 key 长度一次
    size_t key_len = strlen(key);
    if (key_len == 0) return -1;

    const char *p = query;
    while (*p) {
        // 跳过前导 '&' 或起始位置
        if (*p == '&') {
            p++;
            continue;
        }

        // 查找 '='
        const char *eq = strchr(p, '=');
        if (!eq) break; // 格式错误，无 =，跳过

        // 检查 key 是否匹配
        if ((size_t)(eq - p) == key_len && strncmp(p, key, key_len) == 0) {
            // 找到匹配项
            const char *value_start = eq + 1;
            const char *next_amp = strchr(value_start, '&');
            size_t value_len = next_amp ? (size_t)(next_amp - value_start) : strlen(value_start);

            // 防止缓冲区溢出
            if (value_len >= out_len) {
                value_len = out_len - 1;
            }

            // 复制并解码
            memcpy(out, value_start, value_len);
            out[value_len] = '\0';
            url_decode_inplace(out);
            return 0;
        }

        // 移动到下一个参数
        p = strchr(eq, '&');
        if (!p) break;
    }

    return -1; // 未找到
}
/**
 * @brief 从 JSON 对象中安全提取字符串字段
 * @param obj: 非 NULL 的 json_object（应为 object 类型）
 * @param key: 要提取的字段名（非 NULL）
 * @param out: 输出缓冲区（非 NULL）
 * @param len: 缓冲区长度（> 0）
 * @return 0 成功，-1 失败（字段不存在、不是字符串、参数无效等）
 */
int extract_json_string(struct json_object *obj, const char *key, char *out, size_t len) {
    if (!obj || !key || !out || len == 0) {
        return -1;
    }

    if (!json_object_is_type(obj, json_type_object)) {
        return -1;
    }

    struct json_object *val = NULL;
    if (!json_object_object_get_ex(obj, key, &val)) {
        return -1; // 字段不存在
    }

    if (!json_object_is_type(val, json_type_string)) {
        return -1; // 字段存在但不是字符串
    }

    const char *str = json_object_get_string(val);
    if (!str) {
        // 理论上不会发生，但防御性编程
        out[0] = '\0';
        return -1;
    }

    size_t str_len = strlen(str);
    if (str_len >= len) {
        str_len = len - 1;
    }
    memcpy(out, str, str_len);
    out[str_len] = '\0';
    return 0;
}