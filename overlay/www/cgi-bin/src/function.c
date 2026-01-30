#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/file.h>
#include <stdbool.h>
#include <json-c/json.h>

#include "function.h"
#include "common.h"
#include "util.h"
#include "error.h"
#include "system.h"
#include "disk.h"
#include "weather.h"
#include "photos.h"
#include "family.h"
#include "control.h"
#include "settings.h"
#include "zigbee_mq.h"
#include "token.h"

extern const char* get_disk_root(void);
//#define VALID_USERNAME "root"
//#define VALID_PASSWORD "root"

#define WEATHER "/development/tmp/weather.json"
#define TEMP_JSON_PATH "/development/tmp/temperature.json"

static void send_json_response(struct json_object *obj) {
    send_json_headers();
    printf("%s\n", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
    json_object_put(obj);
}

//GET
void weather_get(const char *path, const char *body) {
    (void)path;
    (void)body;
    
    int fd;
    off_t fsize;
    char *buffer = NULL;
    ssize_t bytes_read;
    WeatherData wd;
    json_object *resp = NULL;
    const char *output;

    send_json_headers();
    fd = open(WEATHER, O_RDONLY);
    if (fd < 0) {
        send_error_404("Weather cache not found");
        return;
    }
    //加读锁并读取文件内容
    if (flock(fd, LOCK_SH) != 0) {
        close(fd);
        send_error_500("Failed to acquire read lock");
        return;
    }
    //读取整个文件
    fsize = lseek(fd, 0, SEEK_END);
    if (fsize <= 0) {
        flock(fd, LOCK_UN);
        close(fd);
        send_error_404("Weather file is empty");
        return;
    }
    lseek(fd, 0, SEEK_SET);
    buffer = malloc(fsize + 1);
    if (!buffer) {
        flock(fd, LOCK_UN);
        close(fd);
        send_error_500("Memory error");
        return;
    }
    bytes_read = read(fd, buffer, fsize);
    flock(fd, LOCK_UN);
    close(fd);
    if (bytes_read != fsize) {
        free(buffer);
        send_error_500("Incomplete file read");
        return;
    }
    buffer[fsize] = '\0';
    //解析 JSON
    if (parse_weather_json(buffer, &wd) != 0) {
        free(buffer);
        send_error_500("JSON parse failed");
        return;
    }
    free(buffer);
    //构建响应 JSON
    resp = json_object_new_object();
    json_object_object_add(resp, "code", json_object_new_string(wd.code));
    if (wd.valid) {
        json_object_object_add(resp, "weather",      json_object_new_string(wd.weather));
        json_object_object_add(resp, "temperature",  json_object_new_string(wd.temperature));
        json_object_object_add(resp, "feels_like",   json_object_new_string(wd.feels_like));
        json_object_object_add(resp, "humidity",     json_object_new_string(wd.humidity));
        json_object_object_add(resp, "wind_dir",     json_object_new_string(wd.wind_dir));
        json_object_object_add(resp, "wind_scale",   json_object_new_string(wd.wind_scale));
    } else {
        json_object_object_add(resp, "error", json_object_new_string("Weather data invalid"));
    }
    //只输出JSON
    output = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
    printf("%s\n", output);
    json_object_put(resp);
}
void temperature_get(const char *path, const char *body) {
    (void)path;
    (void)body;

    // 打开文件
    int fd = open(TEMP_JSON_PATH, O_RDONLY);
    if (fd < 0) {
        // 构造错误响应
        json_object *resp = json_object_new_object();
        json_object_object_add(resp, "error", json_object_new_string("Temperature data not available"));
        json_object_object_add(resp, "code", json_object_new_int(404));
        
        printf("Content-Type: application/json\r\n\r\n");
        printf("%s\n", json_object_to_json_string(resp));
        json_object_put(resp);
        return;
    }

    // 加共享读锁
    if (flock(fd, LOCK_SH) != 0) {
        close(fd);
        json_object *resp = json_object_new_object();
        json_object_object_add(resp, "error", json_object_new_string("Failed to acquire read lock"));
        json_object_object_add(resp, "code", json_object_new_int(500));
        
        printf("Content-Type: application/json\r\n\r\n");
        printf("%s\n", json_object_to_json_string(resp));
        json_object_put(resp);
        return;
    }

    // 读取整个文件
    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        flock(fd, LOCK_UN);
        close(fd);
        json_object *resp = json_object_new_object();
        json_object_object_add(resp, "error", json_object_new_string("Temperature file is empty"));
        json_object_object_add(resp, "code", json_object_new_int(404));
        
        printf("Content-Type: application/json\r\n\r\n");
        printf("%s\n", json_object_to_json_string(resp));
        json_object_put(resp);
        return;
    }
    lseek(fd, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        flock(fd, LOCK_UN);
        close(fd);
        json_object *resp = json_object_new_object();
        json_object_object_add(resp, "error", json_object_new_string("Memory allocation failed"));
        json_object_object_add(resp, "code", json_object_new_int(500));
        
        printf("Content-Type: application/json\r\n\r\n");
        printf("%s\n", json_object_to_json_string(resp));
        json_object_put(resp);
        return;
    }

    ssize_t bytes_read = read(fd, buffer, size);
    flock(fd, LOCK_UN);
    close(fd);

    if (bytes_read != size) {
        free(buffer);
        json_object *resp = json_object_new_object();
        json_object_object_add(resp, "error", json_object_new_string("Incomplete file read"));
        json_object_object_add(resp, "code", json_object_new_int(500));
        
        printf("Content-Type: application/json\r\n\r\n");
        printf("%s\n", json_object_to_json_string(resp));
        json_object_put(resp);
        return;
    }
    buffer[bytes_read] = '\0';

    // 尝试解析 JSON
    json_object *parsed = json_tokener_parse(buffer);
    free(buffer);

    json_object *response = json_object_new_object();

    if (parsed == NULL) {
        // 解析失败
        json_object_object_add(response, "error", json_object_new_string("Invalid JSON in temperature file"));
        json_object_object_add(response, "code", json_object_new_int(500));
    } else {
        // 提取字段（安全检查类型）
        json_object *h_obj, *t_obj, *ts_obj;
        double humidity = -1.0, temperature = -1.0;
        int64_t timestamp = 0;

        if (json_object_object_get_ex(parsed, "humidity", &h_obj) &&
            json_object_is_type(h_obj, json_type_double)) {
            humidity = json_object_get_double(h_obj);
        }

        if (json_object_object_get_ex(parsed, "temperature", &t_obj) &&
            json_object_is_type(t_obj, json_type_double)) {
            temperature = json_object_get_double(t_obj);
        }

        if (json_object_object_get_ex(parsed, "timestamp", &ts_obj) &&
            json_object_is_type(ts_obj, json_type_int)) {
            timestamp = json_object_get_int64(ts_obj);
        }

        json_object_put(parsed); // 释放原始解析对象

        // 构造成功响应
        if (humidity >= 0 && temperature >= -50) { // 简单有效性检查
            json_object_object_add(response, "humidity", json_object_new_double(humidity));
            json_object_object_add(response, "temperature", json_object_new_double(temperature));
            json_object_object_add(response, "timestamp", json_object_new_int64(timestamp));
            json_object_object_add(response, "success", json_object_new_boolean(true));
        } else {
            json_object_object_add(response, "error", json_object_new_string("Missing or invalid sensor data"));
            json_object_object_add(response, "code", json_object_new_int(500));
        }
    }

    // 输出最终响应
    send_json_headers();
    printf("%s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
    json_object_put(response);
}
void picture_get(const char *path, const char *body){}
void notice_get(const char *path, const char *body){}

void system_get(const char *path, const char *body) {
    // 🔒 1. 验证登录状态
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        // 直接输出 401 错误（不依赖 send_error_401）
        printf("Status: 401 Unauthorized\r\n");
        printf("Content-Type: application/json\r\n\r\n");
        printf("{\"error\":\"Unauthorized\"}\n");
        return;
    }
    free(token);

    // 📊 2. 采集系统信息
    system_info_t info;
    if (!collect_system_info(&info)) {
        // 直接输出 500 错误
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: application/json\r\n\r\n");
        printf("{\"error\":\"Failed to collect system data\"}\n");
        return;
    }

    // 🧱 3. 构建 JSON 响应
    send_json_headers();
    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "load_percent",     json_object_new_double(info.load_percent));
    json_object_object_add(root, "uptime_minutes",   json_object_new_int64(info.uptime_minutes));
    json_object_object_add(root, "memory_total_mb",  json_object_new_int64(info.memory_total_mb));
    json_object_object_add(root, "memory_used_mb",   json_object_new_int64(info.memory_used_mb));
    json_object_object_add(root, "memory_percent",   json_object_new_double(info.memory_percent));
    json_object_object_add(root, "ip",               json_object_new_string(info.ip));
    if (info.cpu_temp_c >= 0) {
        json_object_object_add(root, "cpu_temp_c",   json_object_new_double(info.cpu_temp_c));
    }
    json_object_object_add(root, "disk_total_gb",    json_object_new_int64(info.disk_total_gb));
    json_object_object_add(root, "disk_used_gb",     json_object_new_int64(info.disk_used_gb));
    json_object_object_add(root, "disk_percent",     json_object_new_double(info.disk_percent));
    printf("%s\n", json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}



void disk_download_get(const char *path, const char *body) {
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    char requested_path[512] = "";
    const char *qs = getenv("QUERY_STRING");
    if (!qs || !strstr(qs, "path=")) {
        send_error_400("Missing 'path' parameter");
        return;
    }
    sscanf(qs, "path=%511[^&\r\n]", requested_path);
    url_decode(requested_path, requested_path);

    if (!is_safe_relative_path(requested_path)) {
        send_error_403("Invalid path");
        return;
    }

    char real_path[1024];
    snprintf(real_path, sizeof(real_path), "%s%s", get_disk_root(), requested_path);

    struct stat st;
    if (stat(real_path, &st) != 0) {
        send_error_404("File not found");
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        send_error_400("Cannot download a directory");
        return;
    }

    // MIME 类型与是否 inline 的判断
    const char *mime = "application/octet-stream";
    const char *disposition = "attachment"; // 默认强制下载

    char *dot = strrchr(real_path, '.');
    if (dot) {
        dot++; // 跳过 '.'
        if (strcasecmp(dot, "txt") == 0) {
            mime = "text/plain";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "jpg") == 0 || strcasecmp(dot, "jpeg") == 0) {
            mime = "image/jpeg";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "png") == 0) {
            mime = "image/png";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "gif") == 0) {
            mime = "image/gif";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "bmp") == 0) {
            mime = "image/bmp";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "webp") == 0) {
            mime = "image/webp";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "svg") == 0) {
            mime = "image/svg+xml";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "pdf") == 0) {
            mime = "application/pdf";
            disposition = "inline"; // 现代浏览器可直接预览 PDF
        }
        else if (strcasecmp(dot, "mp4") == 0) {
            mime = "video/mp4";
            disposition = "inline";
        }
        else if (strcasecmp(dot, "mp3") == 0) {
            mime = "audio/mpeg";
            disposition = "inline";
        }
        // 其他格式保持 attachment（如 .doc, .docx, .zip, .exe）
    }

    FILE *fp = fopen(real_path, "rb");
    if (!fp) {
        send_error_500("Cannot open file");
        return;
    }

    // 输出 HTTP 头
    printf("Content-Type: %s\r\n", mime);
    printf("Content-Length: %ld\r\n", st.st_size);

    char *basename = strrchr(real_path, '/') ? strrchr(real_path, '/') + 1 : real_path;
    printf("Content-Disposition: %s; filename=\"%s\"\r\n", disposition, basename);
    printf("\r\n"); // 空行结束头

    // 发送文件内容
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(fp);
}
void disk_list_get(const char *path, const char *body) {
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    char requested_path[512] = "/";
    const char *qs = getenv("QUERY_STRING");
    if (qs && strstr(qs, "path=")) {
        sscanf(qs, "path=%511[^&\r\n]", requested_path);
        url_decode(requested_path, requested_path);
    }

    if (!is_safe_relative_path(requested_path)) {
        send_error_403("Invalid path");
        return;
    }

    char real_path[1024];
    snprintf(real_path, sizeof(real_path), "%s%s", get_disk_root(), requested_path);

    disk_file_t files[100];
    int count = list_files_in_dir(real_path, files, 100);
    if (count < 0) {
        send_error_500("Failed to read directory");
        return;
    }

    struct json_object *root = json_object_new_object();
    struct json_object *arr = json_object_new_array();

    for (int i = 0; i < count; i++) {
        struct json_object *item = json_object_new_object();
        json_object_object_add(item, "name", json_object_new_string(files[i].name));
        json_object_object_add(item, "size", json_object_new_int64(files[i].size));
        json_object_object_add(item, "is_dir", json_object_new_boolean(files[i].is_dir));
        json_object_object_add(item, "mtime", json_object_new_int64((int64_t)files[i].mtime));
        json_object_array_add(arr, item);
    }

    json_object_object_add(root, "files", arr);
    send_json_response(root);
}
void photos_list_get(const char *path, const char *body) {
    (void)path; (void)body;

    // 认证（可选：如果相册公开可移除）
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    // 复用你已有的函数
    char *json = photos_build_list_json();
    photos_send_json_response(json);
    free(json);
}
void photos_photo_get(const char *path, const char *body) {
    (void)path; (void)body;

    // 1. 认证（可选）
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    // 2. 从 QUERY_STRING 获取 name 参数
    const char *qs = getenv("QUERY_STRING");
    if (!qs || !strstr(qs, "name=")) {
        send_error_400("Missing 'name' parameter");
        return;
    }

    char filename[256] = {0};
    sscanf(qs, "name=%255[^&\r\n]", filename);
    url_decode(filename, filename);
    /*
    // 3. 安全检查：只允许安全文件名（无 / \ .. 等）
    if (!photos_is_safe_filename(filename)) {
        send_error_403("Invalid filename");
        return;
    }
    */

    // 4. 构建完整路径
    char filepath[512];
    if ((size_t)snprintf(filepath, sizeof(filepath), "%s/%s", PHOTOS_DIR, filename) >= sizeof(filepath)) {
        send_error_400("Path too long");
        return;
    }

    // 5. 检查文件是否存在
    struct stat st;
    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        send_error_404("Photo not found");
        return;
    }

    // 6. 确定 MIME 类型（和 disk_download_get 一致）
    const char *mime = "application/octet-stream";
    const char *ext = strrchr(filename, '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "png") == 0) mime = "image/png";
        else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) mime = "image/jpeg";
        else if (strcasecmp(ext, "gif") == 0) mime = "image/gif";
        else if (strcasecmp(ext, "webp") == 0) mime = "image/webp";
        else if (strcasecmp(ext, "bmp") == 0) mime = "image/bmp";
        else if (strcasecmp(ext, "svg") == 0) mime = "image/svg+xml";
    }

    // 7. 打开并发送文件
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send_error_500("Cannot open photo");
        return;
    }

    printf("Content-Type: %s\r\n", mime);
    printf("Content-Length: %ld\r\n", (long)st.st_size);
    printf("Content-Disposition: inline; filename=\"%s\"\r\n", filename);
    printf("\r\n");

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(fp);
}
void photo_note_get(const char *path, const char *body) {
    (void)path; (void)body;

    // 1. 验证 token
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    // 2. 获取查询字符串
    const char *qs = getenv("QUERY_STRING");
    if (!qs || !strstr(qs, "name=")) {
        send_error_400("Missing 'name' parameter");
        return;
    }

    // 3. 提取原始（URL 编码）的 filename
    char raw_filename[256] = {0};
    parse_query_string(qs, "name", raw_filename, sizeof(raw_filename));

    // 4. ✅ 关键修复：URL 解码
    char decoded_filename[256] = {0};
    url_decode(raw_filename, decoded_filename);
    /*
    // 5. 安全校验（使用解码后的名字）
    if (!photos_is_safe_filename(decoded_filename)) {
        send_error_403("Invalid filename");
        return;
    }
    */
    // 6. 构建注释文件路径
    char notepath[512];
    int len = snprintf(notepath, sizeof(notepath), "%s/%s.txt", PHOTOS_DIR, decoded_filename);
    if (len >= (int)sizeof(notepath)) {
        send_error_400("Filename too long");
        return;
    }

    // 7. 尝试打开注释文件
    FILE *fp = fopen(notepath, "r");
    if (!fp) {
        // 文件不存在 → 返回空内容（合法，200 OK）
        printf("Content-Type: text/plain\r\n\r\n");
        return;
    }

    // 8. 返回文件内容（确保 UTF-8 兼容）
    printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(fp);
}
//PUT
void picture_put(const char *path, const char *body){}

//POST
void login_post(const char *path, const char *body) {
    if (!body) {
        send_error_400("Missing request body");
        return;
    }

    json_tokener *tok = json_tokener_new();
    json_object *input = json_tokener_parse_ex(tok, body, -1);
    enum json_tokener_error jerr = json_tokener_get_error(tok);
    json_tokener_free(tok);

    if (!input || jerr != json_tokener_success) {
        send_error_400("Invalid JSON format");
        if (input) json_object_put(input);
        return;
    }

    const char *username = NULL;
    const char *password = NULL;

    json_object *user_obj, *pass_obj;
    if (json_object_object_get_ex(input, "username", &user_obj)) {
        username = json_object_get_string(user_obj);
    }
    if (json_object_object_get_ex(input, "password", &pass_obj)) {
        password = json_object_get_string(pass_obj);
    }

    if (!username || !password) {
        json_object_put(input);
        send_error_400("Missing 'username' or 'password'");
        return;
    }

    // ✅ 从文件加载真实凭据
    char stored_user[128] = {0};
    char stored_pass[128] = {0};

    int auth_success = 0;
    if (load_stored_credentials(stored_user, sizeof(stored_user),
                                stored_pass, sizeof(stored_pass)) == 0) {
        // 比对用户名和密码（明文）
        if (strcmp(username, stored_user) == 0 &&
            strcmp(password, stored_pass) == 0) {
            auth_success = 1;
        }
    }

    json_object *resp = json_object_new_object();

    if (auth_success) {
        char token[33];
        generate_token(token, sizeof(token));

        if (!add_token(token)) {
            json_object_object_add(resp, "success", json_object_new_boolean(0));
            json_object_object_add(resp, "message", json_object_new_string("服务器内部错误"));
            
            printf("Content-Type: application/json\r\n\r\n");
            printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));
            
            json_object_put(input);
            json_object_put(resp);
            return;
        }

        printf("Set-Cookie: token=%s; Path=/; HttpOnly; SameSite=Strict\r\n", token);
        json_object_object_add(resp, "success", json_object_new_boolean(1));
        json_object_object_add(resp, "message", json_object_new_string("登录成功"));
    } else {
        json_object_object_add(resp, "success", json_object_new_boolean(0));
        json_object_object_add(resp, "message", json_object_new_string("用户名或密码错误"));
    }

    printf("Content-Type: application/json\r\n\r\n");
    printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));

    json_object_put(input);
    json_object_put(resp);
}
//check token
void check_auth_get(const char *path, const char *body) {
    // 1. 从 Cookie 中提取 token
    char *token = get_token_from_cookie();
    
    // 2. 验证 token 是否有效（存在 + 未过期）
    if (!token || !is_valid_token(token)) {
        free(token); // 注意：get_token_from_cookie 返回 malloc 内存
        
        // 3. 无效 → 返回 401 Unauthorized
        printf("Status: 401 Unauthorized\r\n");
        printf("Content-Type: application/json\r\n\r\n");
        printf("{\"error\":\"Unauthorized\"}\n");
        return;
    }

    // 4. 有效 → 返回 200 OK（空 JSON 即可）
    free(token);
    printf("Content-Type: application/json\r\n\r\n");
    printf("{}\n");
}

void disk_upload_post(const char *path, const char *body) {
    (void)path;
    (void)body;

    // 1. 验证 token
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    // 2. 从 QUERY_STRING 获取 target_path
    const char *query = getenv("QUERY_STRING");
    if (!query) {
        send_error_400("Missing query string");
        return;
    }

    char qcopy[1024];
    strncpy(qcopy, query, sizeof(qcopy) - 1);
    qcopy[sizeof(qcopy) - 1] = '\0';

    char target_path[512] = "/";
    char *pair = strtok(qcopy, "&");
    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            char key[256], val[256];
            url_decode(pair, key);    
            url_decode(eq + 1, val);
            if (strcmp(key, "path") == 0) {
                if (val[0] != '/') {
                    send_error_400("Path must be absolute");
                    return;
                }
                strncpy(target_path, val, sizeof(target_path) - 1);
                target_path[sizeof(target_path) - 1] = '\0';
            }
        }
        pair = strtok(NULL, "&");
    }

    // 3. 安全校验路径
    if (!is_safe_relative_path(target_path)) {
        send_error_403("Invalid or unsafe path");
        return;
    }

    // 4. 获取 Content-Length 和 Content-Type
    const char *content_length_str = getenv("CONTENT_LENGTH");
    const char *content_type = getenv("CONTENT_TYPE");
    if (!content_length_str || !content_type) {
        send_error_400("Missing headers");
        return;
    }

    long content_length = atol(content_length_str);
    if (content_length <= 0 || content_length > 100 * 1024 * 1024) {
        send_error_400("File too large");
        return;
    }

    const char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start) {
        send_error_400("No boundary in Content-Type");
        return;
    }

    boundary_start += 9;
    char boundary[256];
    const char *end = strchr(boundary_start, ';');
    size_t b_len = end ? (size_t)(end - boundary_start) : strlen(boundary_start);
    if (b_len >= sizeof(boundary)) {
        send_error_400("Boundary too long");
        return;
    }
    strncpy(boundary, boundary_start, b_len);
    boundary[b_len] = '\0';

    // 5. 读取整个请求体
    char *full_body = malloc(content_length + 1);
    if (!full_body) {
        send_error_500("Out of memory");
        return;
    }
    
    if (read(0, full_body, content_length) != content_length) {
        free(full_body);
        send_error_500("Read request body failed");
        return;
    }
   
    full_body[content_length] = '\0';

    // 6. 构建目标目录路径
    const char *disk_root = get_disk_root();
    if (!disk_root) {
        free(full_body);
        send_error_500("Disk root not configured");
        return;
    }

    char save_dir[1024];
    if (strcmp(target_path, "/") == 0) {
        snprintf(save_dir, sizeof(save_dir), "%s", disk_root);
    } else {
        snprintf(save_dir, sizeof(save_dir), "%s%s", disk_root, target_path);
    }

    // 7. 解析 multipart 并保存
    int result = parse_multipart_and_save(full_body, content_length, boundary, save_dir);
    free(full_body);

    if (result == -1) {
        send_error_400("Malformed multipart data");
    } else if (result == -2) {
        send_error_400("Unsafe filename");
    } else if (result == -3) {
        send_error_500("Failed to write file");
    } else {
        json_success("File uploaded successfully");
    }
}

void disk_mkdir_post(const char *path, const char *body) {
    (void)path;

    char input_name_raw[256] = {0};
    char input_path_raw[1024] = {0};
    char input_name[256] = {0};
    char input_path[1024] = {0};

    // 提取 name
    const char *name_start = strstr(body, "name=");
    if (!name_start) {
        send_error_400("Missing 'name'");
        return;
    }
    name_start += 5;
    const char *name_end = strchr(name_start, '&');
    int name_len = name_end ? (int)(name_end - name_start) : (int)strlen(name_start);
    if (name_len > 0 && name_len < (int)sizeof(input_name_raw)) {
        memcpy(input_name_raw, name_start, name_len);
        input_name_raw[name_len] = '\0';
    }

    // 提取 path（可选）
    const char *path_start = strstr(body, "path=");
    if (path_start) {
        path_start += 5;
        const char *path_end = strchr(path_start, '&');
        int path_len = path_end ? (int)(path_end - path_start) : (int)strlen(path_start);
        if (path_len > 0 && path_len < (int)sizeof(input_path_raw)) {
            memcpy(input_path_raw, path_start, path_len);
            input_path_raw[path_len] = '\0';
        }
    }

    // URL 解码
    url_decode(input_name, input_name_raw);
    url_decode(input_path, input_path_raw);

    // >>> 关键修复：清理 path，将 "/" 或 "/xxx" 转为合法相对路径 <<<
    if (input_path[0] == '/') {
        char *p = input_path;
        while (*p == '/') p++;  // 跳过所有前导 /
        if (*p == '\0') {
            // 只有 "/" → 视为空路径（根目录）
            input_path[0] = '\0';
        } else {
            // 如 "/docs" → 变成 "docs"
            memmove(input_path, p, strlen(p) + 1);
        }
    }

    // 调试日志（上线可删，现在保留）
    char debug_msg[600];
    int len = snprintf(debug_msg, sizeof(debug_msg),
                       "[MKDIR FINAL] name='%s', path='%s'\n", input_name, input_path);
    write(2, debug_msg, len);

    // 检查文件夹名
    if (strlen(input_name) == 0) {
        send_error_400("Folder name is empty");
        return;
    }
    if (strcmp(input_name, ".") == 0 || strcmp(input_name, "..") == 0) {
        send_error_403("Invalid folder name");
        return;
    }
    if (strchr(input_name, '/') || strchr(input_name, '\\')) {
        send_error_403("Folder name contains '/' or '\\'");
        return;
    }

    // 检查 path（现在已无前导 /，只防 .. 和 \）
    if (input_path[0] != '\0') {
        if (strstr(input_path, "..") || strchr(input_path, '\\')) {
            send_error_403("Invalid path");
            return;
        }
    }

    // 构建完整路径
    char full_dir[2048];
    const char *disk_root = get_disk_root();
    if (input_path[0] != '\0') {
        snprintf(full_dir, sizeof(full_dir), "%s/%s/%s", disk_root, input_path, input_name);
    } else {
        snprintf(full_dir, sizeof(full_dir), "%s/%s", disk_root, input_name);
    }

    // 创建目录
    if (mkpath(full_dir, 0777) != 0) {
        len = snprintf(debug_msg, sizeof(debug_msg),
                       "[MKDIR ERROR] %s (errno=%d)\n", strerror(errno), errno);
        write(2, debug_msg, len);
        send_error_500("Create directory failed");
        return;
    }

    json_success("Folder created");
}
// disk_rename_post: 处理 POST /disk/rename
void disk_rename_post(const char *path, const char *body) {
    (void)path; // 实际路径从 body 中解析

    char req_path[1024] = {0};
    char old_name[512] = {0};
    char new_name[512] = {0};

    if (!parse_rename_json(body, req_path, old_name, new_name)) {
        send_error_400("Invalid JSON");
        return;
    }

    const char *disk_root = get_disk_root();
    if (!disk_root) {
        send_error_500("Disk root not configured");
        return;
    }

    // 确保 req_path 是绝对路径（以 / 开头）
    if (req_path[0] != '/') {
        send_error_400("Path must start with '/'");
        return;
    }

    // 使用大缓冲区拼接完整路径
    char full_old_path[4096];
    char full_new_path[4096];

    if (snprintf(full_old_path, sizeof(full_old_path), "%s%s/%s",
                 disk_root, req_path, old_name) >= (int)sizeof(full_old_path)) {
        send_error_400("Old path too long");
        return;
    }

    if (snprintf(full_new_path, sizeof(full_new_path), "%s%s/%s",
                 disk_root, req_path, new_name) >= (int)sizeof(full_new_path)) {
        send_error_400("New path too long");
        return;
    }

    // 检查源文件是否存在
    if (access(full_old_path, F_OK) != 0) {
        send_error_404("Source file not found");
        return;
    }

    // 如果目标已存在，先删除（避免 rename 失败）
    if (access(full_new_path, F_OK) == 0) {
        unlink(full_new_path);
    }

    // 执行重命名（直接传递原始字节，支持中文）
    if (rename(full_old_path, full_new_path) == 0) {
        json_success("Rename OK");
    } else {
        send_error_500("System rename() failed");
    }
}
void photos_upload(const char *path, const char *body) {
    (void)path;
    (void)body;

    // 可选：验证 token（如果相册需要权限）
    /*
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);
    */

    // 1. 获取 Content-Length
    const char *content_length_str = getenv("CONTENT_LENGTH");
    const char *content_type = getenv("CONTENT_TYPE");
    if (!content_length_str || !content_type) {
        send_error_400("Missing Content-Length or Content-Type");
        return;
    }

    long content_length = atol(content_length_str);
    if (content_length <= 0 || content_length > 20 * 1024 * 1024) { // 限制 20MB
        send_error_400("File too large");
        return;
    }

    // 2. 提取 boundary（健壮版）
    const char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start) {
        send_error_400("No boundary in Content-Type");
        return;
    }
    boundary_start += 9;

    char boundary[256];
    size_t b_len;
    if (*boundary_start == '"') {
        boundary_start++;
        const char *end_quote = strchr(boundary_start, '"');
        b_len = end_quote ? (size_t)(end_quote - boundary_start) : strlen(boundary_start);
    } else {
        const char *end_semi = strchr(boundary_start, ';');
        b_len = end_semi ? (size_t)(end_semi - boundary_start) : strlen(boundary_start);
    }

    if (b_len == 0 || b_len >= sizeof(boundary)) {
        send_error_400("Invalid boundary");
        return;
    }
    strncpy(boundary, boundary_start, b_len);
    boundary[b_len] = '\0';

    // 3. 读取完整请求体
    char *full_body = malloc(content_length + 1);
    if (!full_body) {
        send_error_500("Out of memory");
        return;
    }

    ssize_t total = 0;
    ssize_t n;
    while (total < content_length) {
        n = read(STDIN_FILENO, full_body + total, content_length - total);
        if (n <= 0) break;
        total += n;
    }

    if (total != content_length) {
        free(full_body);
        send_error_500("Read request body failed");
        return;
    }
    full_body[content_length] = '\0'; // 仅用于调试打印，实际解析用二进制

    // 4. 设置保存目录（相册专用）
    const char *photos_dir = "/media/sdcard/photos";
    // 确保目录存在（可选）
    // mkdir(photos_dir, 0755);

    // 5. 复用 multipart 解析函数
    int result = parse_multipart_and_save(full_body, content_length, boundary, photos_dir);
    free(full_body);

    // 6. 返回结果
    if (result == -1) {
        send_error_400("Malformed multipart data");
    } else if (result == -2) {
        send_error_400("Unsafe filename");
    } else if (result == -3) {
        send_error_500("Failed to write file");
    } else {
        json_success("Photo uploaded successfully");
    }
}
/*
void photo_note_post(const char *path, const char *body) {
    (void)path;

    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    // 用于存储解析出的原始数据（JSON中的原始内容）
    char raw_filename[512] = {0}; // 增大缓冲区以容纳URL编码后的长字符串
    char note[1024] = {0};

    // 解析JSON
    extract_json_string(body, "filename", raw_filename, sizeof(raw_filename));
    extract_json_string(body, "note", note, sizeof(note));

    // --- 关键修改：使用你提供的 url_decode ---
    // 定义解码后的文件名缓冲区
    char decoded_filename[256] = {0};
    
    // 调用你提供的函数进行解码
    // raw_filename 是输入（如： %E9%AB%98%E5%B1%B1.png）
    // decoded_filename 是输出（如： 高山.png）
    photo_url_decode(raw_filename, decoded_filename);
    //end_error_403("test url decode");
    /*
    // 安全校验（现在传入的是解码后的中文名，但你的函数允许非ASCII字符）
    if (!photos_is_safe_filename(decoded_filename)) {
        send_error_403("Invalid filename");
        return;
    }

    // 构建路径并保存
    char notepath[512];
    snprintf(notepath, sizeof(notepath), "%s/%s.txt", PHOTOS_DIR, decoded_filename);

    FILE *fp = fopen(notepath, "w");
    if (!fp) {
        send_error_500("Cannot save note");
        return;
    }
    fprintf(fp, "%s", note);
    fclose(fp);

    json_success("Note saved");
}
*/
void photo_note_post(const char *path, const char *body) {
    (void)path;

    if (!body || body[0] == '\0') {
        send_error_400("Empty request body");
        return;
    }

    // 验证 token
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    // 🔑 关键：先解析整个 JSON 字符串
    struct json_object *jobj = json_tokener_parse(body);
    if (!jobj) {
        send_error_400("Invalid JSON format");
        return;
    }

    // 提取字段
    char raw_filename[512] = {0};
    char note[1024] = {0};

    if (extract_json_string(jobj, "filename", raw_filename, sizeof(raw_filename)) != 0 ||
        extract_json_string(jobj, "note", note, sizeof(note)) != 0) {
        json_object_put(jobj);
        send_error_400("Missing or invalid 'filename' or 'note'");
        return;
    }

    json_object_put(jobj); // 释放 JSON 对象

    // 调试输出
    fprintf(stderr, "DEBUG: raw_filename = [%s]\n", raw_filename);
    fprintf(stderr, "DEBUG: note = [%s]\n", note);

    // URL 解码
    char decoded_filename[256] = {0};
    photo_url_decode(raw_filename, decoded_filename);

    fprintf(stderr, "DEBUG: decoded_filename = [%s]\n", decoded_filename);

    if (decoded_filename[0] == '\0' || decoded_filename[0] == '.') {
        send_error_400("Invalid decoded filename");
        return;
    }

    // 保存
    char notepath[512];
    snprintf(notepath, sizeof(notepath), "%s/%s.txt", PHOTOS_DIR, decoded_filename);

    FILE *fp = fopen(notepath, "w");
    if (!fp) {
        send_error_500("Cannot open note file for writing");
        return;
    }
    fputs(note, fp);
    fclose(fp);

    json_success("Note saved");
}
//DELETE
void disk_delete_handler(const char *path, const char *body) {
    char *token = get_token_from_cookie();
    if (!token || !is_valid_token(token)) {
        free(token);
        send_error_401("Unauthorized");
        return;
    }
    free(token);

    char requested_path[512] = "";
    const char *qs = getenv("QUERY_STRING");
    if (!qs || !strstr(qs, "path=")) {
        send_error_400("Missing 'path' parameter");
        return;
    }
    sscanf(qs, "path=%511[^&\r\n]", requested_path);
    url_decode(requested_path, requested_path);

    if (!is_safe_relative_path(requested_path)) {
        send_error_403("Invalid path");
        return;
    }

    char real_path[1024];
    snprintf(real_path, sizeof(real_path), "%s%s", get_disk_root(), requested_path);

    if (delete_path_recursive(real_path) != 0) {
        send_error_500("Delete failed");
        return;
    }

    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "message", json_object_new_string("Deleted successfully"));
    send_json_response(root);
}
void photos_delete(const char *path, const char *body) {
    (void)path;

    if (!body || strlen(body) == 0) {
        photos_send_json_response("{\"error\":\"Request body is empty\"}");
        return;
    }

    char *filename = photos_extract_filename_from_json(body);
    if (!filename) {
        photos_send_json_response("{\"error\":\"Invalid JSON: missing or invalid 'filename' field\"}");
        return;
    }

    if (photos_delete_photo_file(filename)) {
        photos_send_json_response("{\"code\":\"200\",\"msg\":\"Photo deleted successfully\"}");
    } else {
        photos_send_json_response("{\"error\":\"Failed to delete photo (file not found or unsafe name)\"}");
    }

    free(filename); // 由 strdup 分配
}















void family_members_get(const char *path, const char *body) {
    (void)path; (void)body;

    json_object *root = load_family_data();
    send_json_headers();
    if (!root) {
        printf("{\"error\":\"Internal error\"}\n");
        return;
    }
    json_object *resp = json_object_new_object();
    json_object_object_add(resp, "members",
        json_object_get(json_object_object_get(root, "members")));
    printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));
    json_object_put(resp);
    json_object_put(root);
}

void family_members_post(const char *path, const char *body) {
    (void)path;

    json_object *req = json_tokener_parse(body);
    send_json_headers();
    if (!req) {
        printf("{\"error\":\"Invalid JSON\"}\n");
        return;
    }
    const char *name = json_object_get_string(json_object_object_get(req, "name"));
    const char *wechat_id = json_object_get_string(json_object_object_get(req, "wechat_id"));
    const char *birthday = json_object_get_string(json_object_object_get(req, "birthday"));
    if (!name || !wechat_id || !birthday) {
        json_object_put(req);
        printf("{\"error\":\"Missing name/wechat_id/birthday\"}\n");
        return;
    }
    json_object *root = load_family_data();
    if (!root) {
        json_object_put(req);
        printf("{\"error\":\"Failed to load data\"}\n");
        return;
    }
    json_object *members = json_object_object_get(root, "members");
    int exists = 0;
    for (int i = 0; i < json_object_array_length(members); i++) {
        json_object *m = json_object_array_get_idx(members, i);
        const char *n = json_object_get_string(json_object_object_get(m, "name"));
        if (n && strcmp(n, name) == 0) {
            exists = 1;
            break;
        }
    }
    if (exists) {
        json_object_put(req);
        json_object_put(root);
        printf("{\"error\":\"Member already exists\"}\n");
        return;
    }
    json_object *new_member = json_object_new_object();
    json_object_object_add(new_member, "name", json_object_new_string(name));
    json_object_object_add(new_member, "wechat_id", json_object_new_string(wechat_id));
    json_object_object_add(new_member, "birthday", json_object_new_string(birthday));
    json_object_object_add(new_member, "tasks", json_object_new_array());
    json_object_array_add(members, new_member);
    if (save_family_data(root) != 0) {
        json_object_put(req);
        json_object_put(root);
        printf("{\"error\":\"Failed to save\"}\n");
        return;
    }
    json_object_put(req);
    json_object_put(root);
    printf("{\"status\":\"success\"}\n");
}

void family_task_post(const char *path, const char *body) {
    (void)path;

    send_json_headers();

    json_object *req = json_tokener_parse(body);
    if (!req) {
        printf("{\"error\":\"Invalid JSON\"}\n");
        return;
    }

    const char *target = json_object_get_string(json_object_object_get(req, "target_member"));
    const char *title = json_object_get_string(json_object_object_get(req, "title"));
    const char *due_date = json_object_get_string(json_object_object_get(req, "due_date"));

    if (!target || !title || !due_date) {
        json_object_put(req);
        printf("{\"error\":\"Missing target/title/due_date\"}\n");
        return;
    }

    json_object *root = load_family_data();
    if (!root) {
        json_object_put(req);
        printf("{\"error\":\"Failed to load data\"}\n");
        return;
    }

    json_object *members = json_object_object_get(root, "members");
    json_object *target_member = NULL;

    for (int i = 0; i < json_object_array_length(members); i++) {
        json_object *m = json_object_array_get_idx(members, i);
        const char *n = json_object_get_string(json_object_object_get(m, "name"));
        if (n && strcmp(n, target) == 0) {
            target_member = m;
            break;
        }
    }

    if (!target_member) {
        json_object_put(req);
        json_object_put(root);
        printf("{\"error\":\"Target member not found\"}\n");
        return;
    }

    json_object *tasks = json_object_object_get(target_member, "tasks");
    if (!tasks || !json_object_is_type(tasks, json_type_array)) {
        tasks = json_object_new_array();
        json_object_object_add(target_member, "tasks", tasks);
    }

    json_object *task = json_object_new_object();
    json_object_object_add(task, "title", json_object_new_string(title));
    json_object_object_add(task, "due_date", json_object_new_string(due_date));
    json_object_object_add(task, "creator", json_object_new_string("anonymous"));
    json_object_object_add(task, "notified", json_object_new_boolean(0)); // ← 关键！false
    json_object_array_add(tasks, task);

    if (save_family_data(root) != 0) {
        json_object_put(req);
        json_object_put(root);
        printf("{\"error\":\"Failed to save task\"}\n");
        return;
    }

    json_object_put(req);
    json_object_put(root);
    printf("{\"status\":\"success\"}\n");
}

void family_my_tasks_get(const char *path, const char *body) {
    (void)path;
    (void)body;

    send_json_headers();

    json_object *root = load_family_data();
    if (!root) {
        printf("[]\n");
        return;
    }

    time_t now = time(NULL);
    json_object *result = json_object_new_array();

    json_object *members = json_object_object_get(root, "members");
    if (members && json_object_is_type(members, json_type_array)) {
        for (int i = 0; i < json_object_array_length(members); i++) {
            json_object *member = json_object_array_get_idx(members, i);
            const char *member_name = json_object_get_string(
                json_object_object_get(member, "name")
            );
            // 如果 name 不存在，跳过该成员
            if (!member_name) continue;

            json_object *tasks = json_object_object_get(member, "tasks");
            if (tasks && json_object_is_type(tasks, json_type_array)) {
                for (int j = 0; j < json_object_array_length(tasks); j++) {
                    json_object *task = json_object_array_get_idx(tasks, j);
                    const char *due_str = json_object_get_string(
                        json_object_object_get(task, "due_date")
                    );

                    time_t due_time = parse_due_date(due_str);
                    if (due_time == -1 || due_time <= now) {
                        continue; // 跳过已过期或无效时间
                    }

                    // 创建新任务对象（避免修改原始数据）
                    json_object *output_task = json_object_new_object();

                    // 复制原任务所有字段
                    json_object_object_foreach(task, key, val) {
                        json_object_get(val); // 增加引用
                        json_object_object_add(output_task, key, val);
                    }

                    // 注入 member_name
                    json_object_object_add(output_task, "member_name",
                                         json_object_new_string(member_name));

                    json_object_array_add(result, output_task);
                }
            }
        }
    }

    printf("%s\n", json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN));

    json_object_put(result);
    json_object_put(root);
}










void control_light_put(const char *path, const char *body) {
    int state = parse_state_from_json(body);
    if (state == -1) {
        printf("Status: 400 Bad Request\r\n\r\n");
        return;
    }
    const char* cmd = (state == 1) ? ZIGBEE_CMD_LIGHT_ON : ZIGBEE_CMD_LIGHT_OFF;
    if (cgi_send_zigbee_cmd(cmd) == 0) {
        printf("Status: 200 OK\r\nContent-Type: application/json\r\n\r\n{\"result\":\"success\"}");
    } else {
        printf("Status: 500 Internal Server Error\r\n\r\n");
    }
}

void control_aircon_put(const char *path, const char *body) {
    int state = parse_state_from_json(body);
    if (state == -1) {
        printf("Status: 400 Bad Request\r\n\r\n");
        return;
    }
    const char* cmd = (state == 1) ? ZIGBEE_CMD_AIRCON_ON : ZIGBEE_CMD_AIRCON_OFF;
    if (cgi_send_zigbee_cmd(cmd) == 0) {
        printf("Status: 200 OK\r\nContent-Type: application/json\r\n\r\n{\"result\":\"success\"}");
    } else {
        printf("Status: 500 Internal Server Error\r\n\r\n");
    }
}

void control_washing_machine_put(const char *path, const char *body) {
    int state = parse_state_from_json(body);
    if (state == -1) {
        printf("Status: 400 Bad Request\r\n\r\n");
        return;
    }
    const char* cmd = (state == 1) ? ZIGBEE_CMD_WASHING_ON : ZIGBEE_CMD_WASHING_OFF;
    if (cgi_send_zigbee_cmd(cmd) == 0) {
        printf("Status: 200 OK\r\nContent-Type: application/json\r\n\r\n{\"result\":\"success\"}");
    } else {
        printf("Status: 500 Internal Server Error\r\n\r\n");
    }
}

void control_fan_put(const char *path, const char *body) {
    int state = parse_state_from_json(body);
    if (state == -1) {
        printf("Status: 400 Bad Request\r\n\r\n");
        return;
    }
    const char* cmd = (state == 1) ? ZIGBEE_CMD_FAN_ON : ZIGBEE_CMD_FAN_OFF;
    if (cgi_send_zigbee_cmd(cmd) == 0) {
        printf("Status: 200 OK\r\nContent-Type: application/json\r\n\r\n{\"result\":\"success\"}");
    } else {
        printf("Status: 500 Internal Server Error\r\n\r\n");
    }
}

void control_door_put(const char *path, const char *body) {
    int state = parse_state_from_json(body);
    if (state == -1) {
        printf("Status: 400 Bad Request\r\n\r\n");
        return;
    }
    const char* cmd = (state == 1) ? ZIGBEE_CMD_DOOR_OPEN : ZIGBEE_CMD_DOOR_CLOSE;
    if (cgi_send_zigbee_cmd(cmd) == 0) {
        printf("Status: 200 OK\r\nContent-Type: application/json\r\n\r\n{\"result\":\"success\"}");
    } else {
        printf("Status: 500 Internal Server Error\r\n\r\n");
    }
}

void control_light_get(const char *path, const char *body) {

    int state = 1; // 固定返回“开启”用于测试

    printf("Content-Type: application/json\r\n\r\n");
    printf("{\"state\": %d}", state);
}

void control_aircon_get(const char *path, const char *body) {

    int state = 0; // 固定返回“关闭”

    printf("Content-Type: application/json\r\n\r\n");
    printf("{\"state\": %d}", state);
}

void control_washing_machine_get(const char *path, const char *body) {

    int state = 0; // 固定返回“停止”

    printf("Content-Type: application/json\r\n\r\n");
    printf("{\"state\": %d}", state);
}

void control_fan_get(const char *path, const char *body) {

    int state = 1; // 固定返回“运转中”

    printf("Content-Type: application/json\r\n\r\n");
    printf("{\"state\": %d}", state);
}

void control_door_get(const char *path, const char *body) {

    // 注意：门默认“锁定”对应 state=0（因为前端 door: true 表示锁定）
    // 但为了统一语义，建议：state=1 表示“解锁”，state=0 表示“锁定”
    int state = 0; // 固定返回“锁定”

    printf("Content-Type: application/json\r\n\r\n");
    printf("{\"state\": %d}", state);
}






void settings_change_password_get(const char *path, const char *body) {
    (void)path; (void)body;

    char username[128] = {0};
    read_username(username, sizeof(username));

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "username", json_object_new_string(username));

    send_json_headers();
    printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));
    fflush(stdout);

    json_object_put(resp);
}

// --- POST /home/settings/change-password ---
void settings_change_password_post(const char *path, const char *body) {
    (void)path;

    if (!body || body[0] == '\0') {
        send_json_headers();
        printf("{\"error\":\"Empty request body\"}\n");
        fflush(stdout);
        return;
    }

    struct json_object *jobj = json_tokener_parse(body);
    if (!jobj) {
        send_json_headers();
        printf("{\"error\":\"Invalid JSON\"}\n");
        fflush(stdout);
        return;
    }

    char username[128] = {0};
    char password[128] = {0};

    int ok = (extract_json_string(jobj, "username", username, sizeof(username)) == 0);
    extract_json_string(jobj, "password", password, sizeof(password)); // 允许为空

    json_object_put(jobj);

    if (!ok || username[0] == '\0') {
        send_json_headers();
        printf("{\"error\":\"Username required\"}\n");
        fflush(stdout);
        return;
    }

    if (write_login_file(username, password) != 0) {
        send_json_headers();
        printf("{\"error\":\"Failed to save\"}\n");
        fflush(stdout);
        return;
    }

    send_json_headers();
    printf("{\"status\":\"ok\"}\n");
    fflush(stdout);
}

// --- GET /home/settings/public ---
void settings_public_get(const char *path, const char *body) {
    (void)path; (void)body;

    int enabled = is_ngrok_running();

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "enabled", json_object_new_boolean(enabled));

    send_json_headers();
    printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));
    fflush(stdout);

    json_object_put(resp);
}

// --- PUT /home/settings/public ---
void settings_public_put(const char *path, const char *body) {
    (void)path;

    if (!body || body[0] == '\0') {
        send_json_headers();
        printf("{\"error\":\"Empty request body\"}\n");
        fflush(stdout);
        return;
    }

    struct json_object *jobj = json_tokener_parse(body);
    if (!jobj) {
        send_json_headers();
        printf("{\"error\":\"Invalid JSON\"}\n");
        fflush(stdout);
        return;
    }

    int enabled = 0;
    struct json_object *val;
    if (json_object_object_get_ex(jobj, "enabled", &val)) {
        if (json_object_is_type(val, json_type_boolean)) {
            enabled = json_object_get_boolean(val);
        }
    }

    json_object_put(jobj);

    // 执行控制脚本
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s %s", WEB_TUNNEL_SCRIPT_PATH, enabled ? "start" : "stop");

    int ret = system(cmd);
    fprintf(stderr, "DEBUG: cmd='%s'\n", cmd);
    fprintf(stderr, "DEBUG: system() returned %d, WEXITSTATUS=%d\n", ret, WEXITSTATUS(ret));
    fflush(stderr);
    if (ret == -1 || WEXITSTATUS(ret) != 0) {
        printf("Content-Type: application/json\r\n\r\n{\"error\":\"Tunnel control failed\"}\n");
        fflush(stdout);
        return;
    }

    // 构造响应
    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "status", json_object_new_string("ok"));

    // 仅在开启时读取公网 URL
    if (enabled) {
        char public_url[256] = "";
        FILE *url_file = fopen(WEB_TUNNEL_URL_FILE, "r");
        if (url_file) {
            if (fgets(public_url, sizeof(public_url), url_file)) {
                size_t len = strlen(public_url);
                if (len > 0 && public_url[len - 1] == '\n') {
                    public_url[len - 1] = '\0';
                }
                // ✅ 修复点：检查 ngrok-free. 而非 .ngrok-free.app
                if (strstr(public_url, "https://") && strstr(public_url, "ngrok-free.")) {
                    json_object_object_add(resp, "public_url", json_object_new_string(public_url));
                }
            }
            fclose(url_file);
        }
    }

    send_json_headers();
    printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));
    fflush(stdout);

    json_object_put(resp);
}