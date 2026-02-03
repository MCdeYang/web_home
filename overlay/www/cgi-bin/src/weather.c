/**********************************************************************
 * @file weather.c
 * @brief 天气数据解析函数实现
 *
 * 本文件实现天气 JSON 响应的解析
 * 供 CGI 接口读取缓存并生成结构化天气数据
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 只做解析不负责网络请求与缓存写入
 **********************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <json-c/json.h>
#include <string.h>
#include "weather.h"

// 初始化天气数据结构
static void init_weather_data(WeatherData*wd){
    memset(wd,0,sizeof(WeatherData));
    strcpy(wd->code,"500");
    wd->valid=0;
}

// 安全提取字符串字段
static void get_json_str(json_object*obj,const char*key,char*out,size_t out_len){
    json_object*v;
    const char*s;

    if(!obj||!key||!out||out_len==0){
        return;
    }
    v=json_object_object_get(obj,key);
    if(!v){
        return;
    }
    s=json_object_get_string(v);
    if(!s){
        return;
    }
    strncpy(out,s,out_len-1);
    out[out_len-1]='\0';
}

// 解析天气 JSON
int parse_weather_json(const char*json_str,WeatherData*out){
    json_object*root;
    json_object*code_obj;
    const char*code;
    json_object*now_obj;

    if(!json_str||!out){
        return -1;
    }

    init_weather_data(out);
    root=json_tokener_parse(json_str);
    if(!root){
        return -1;
    }

    code_obj=NULL;
    if(json_object_object_get_ex(root,"code",&code_obj)){
        code=json_object_get_string(code_obj);
        if(code){
            strncpy(out->code,code,sizeof(out->code)-1);
            out->code[sizeof(out->code)-1]='\0';
        }
    }

    if(strcmp(out->code,"200")!=0){
        json_object_put(root);
        return 0;
    }

    now_obj=NULL;
    if(!json_object_object_get_ex(root,"now",&now_obj)){
        json_object_put(root);
        return 0;
    }

    get_json_str(now_obj,"text",out->weather,sizeof(out->weather));
    get_json_str(now_obj,"temp",out->temperature,sizeof(out->temperature));
    get_json_str(now_obj,"feelsLike",out->feels_like,sizeof(out->feels_like));
    get_json_str(now_obj,"humidity",out->humidity,sizeof(out->humidity));
    get_json_str(now_obj,"windDir",out->wind_dir,sizeof(out->wind_dir));
    get_json_str(now_obj,"windScale",out->wind_scale,sizeof(out->wind_scale));

    out->valid=1;
    json_object_put(root);
    return 0;
}
