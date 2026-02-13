#ifndef WEATHER_H
#define WEATHER_H

typedef struct{
    char code[16];
    char weather[64];
    char temperature[16];
    char feels_like[16];
    char humidity[16];
    char wind_dir[32];
    char wind_scale[8];
    int valid;
}WeatherData;

int parse_weather_json(const char*json_str,WeatherData*out);

#endif
