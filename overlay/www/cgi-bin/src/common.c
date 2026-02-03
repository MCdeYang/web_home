/**********************************************************************
 * @file common.c
 * @brief CGI 公共输出函数实现
 *
 * 本文件实现 CGI 通用的响应头输出函数
 * 供各处理函数统一输出 JSON 响应头
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 输出内容类型与缓存控制相关头字段
 **********************************************************************/
#include <stdio.h>
#include "common.h"

// 输出 JSON 响应头
void send_json_headers(void){
    printf("Content-Type: application/json; charset=utf-8\r\n");
    printf("Cache-Control: no-cache\r\n");
    printf("\r\n");
}

// 输出 JSON 对象响应
void send_json_object_response(json_object*obj){
    if(!obj){
        send_json_headers();
        printf("{}\n");
        return;
    }
    send_json_headers();
    printf("%s\n",json_object_to_json_string_ext(obj,JSON_C_TO_STRING_PLAIN));
    json_object_put(obj);
}
