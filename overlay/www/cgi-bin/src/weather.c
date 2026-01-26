#include <stdio.h>
#include <unistd.h>
#include <json-c/json.h>
#include <string.h>
#include "weather.h"

// 初始化 WeatherData 结构体
static void init_weather_data(WeatherData *wd) {
    memset(wd, 0, sizeof(WeatherData));
    strcpy(wd->code, "500");
    wd->valid = 0;
}

// 从 JSON 字符串解析天气数据
int parse_weather_json(const char *json_str, WeatherData *out) {
    if (!json_str || !out) return -1;

    init_weather_data(out);

    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        return -1;
    }

    // 获取 code
    json_object *code_obj;
    if (json_object_object_get_ex(root, "code", &code_obj)) {
        const char *code = json_object_get_string(code_obj);
        if (code) strncpy(out->code, code, sizeof(out->code) - 1);
    }

    // 只有 code == "200" 才继续解析 now
    if (strcmp(out->code, "200") != 0) {
        json_object_put(root);
        return 0; // 解析成功但数据无效
    }

    json_object *now_obj;
    if (!json_object_object_get_ex(root, "now", &now_obj)) {
        json_object_put(root);
        return 0;
    }

    // 安全提取字段（避免 NULL）
    #define SAFE_GET_STR(field, key, max_len) do { \
        json_object *obj = json_object_object_get(now_obj, key); \
        if (obj) { \
            const char *s = json_object_get_string(obj); \
            if (s) strncpy((field), s, (max_len) - 1); \
        } \
    } while(0)

    SAFE_GET_STR(out->weather, "text", sizeof(out->weather));
    SAFE_GET_STR(out->temperature, "temp", sizeof(out->temperature));
    SAFE_GET_STR(out->feels_like, "feelsLike", sizeof(out->feels_like));
    SAFE_GET_STR(out->humidity, "humidity", sizeof(out->humidity));
    SAFE_GET_STR(out->wind_dir, "windDir", sizeof(out->wind_dir));
    SAFE_GET_STR(out->wind_scale, "windScale", sizeof(out->wind_scale));

    out->valid = 1;
    json_object_put(root);
    return 0;
}