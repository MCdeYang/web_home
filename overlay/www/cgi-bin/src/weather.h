#ifndef WEATHER_H
#define WEATHER_H

typedef struct {
    char code[16];
    char weather[64];       // 如 "晴"、"雾"
    char temperature[16];   // 如 "-28"
    char feels_like[16];    // 如 "-33"
    char humidity[16];      // 如 "70"
    char wind_dir[32];      // 如 "西南风"
    char wind_scale[8];     // 如 "1"
    int valid;              // 是否成功解析
} WeatherData;

int parse_weather_json(const char *json_str, WeatherData *out);

#endif