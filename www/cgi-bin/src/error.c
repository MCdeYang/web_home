/**********************************************************************
 * @file error.c
 * @brief CGI 错误响应输出实现
 *
 * 本文件实现统一的 JSON 成功与错误响应输出
 * 供各 CGI 处理函数在异常场景下返回错误信息
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 输出包含状态行与 JSON 响应体
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "error.h"

// 输出成功响应
void json_success(const char*msg){
    json_object*obj;

    obj=json_object_new_object();
    json_object_object_add(obj,"message",json_object_new_string(msg?msg:""));
    printf("Content-Type: application/json\r\n\r\n");
    printf("%s\n",json_object_to_json_string_ext(obj,JSON_C_TO_STRING_PLAIN));
    json_object_put(obj);
}

// 输出错误响应
void _send_json_error(int status_code,const char*status_text,const char*message){
    json_object*obj;
    const char*json_str;

    if(status_code!=200){
        fprintf(stdout,"Status: %d %s\r\n",status_code,status_text);
    }
    fputs("Content-Type: application/json\r\n\r\n",stdout);
    obj=json_object_new_object();
    json_object_object_add(obj,"error",json_object_new_string(message?message:""));
    json_object_object_add(obj,"code",json_object_new_int(status_code));
    json_str=json_object_to_json_string_ext(obj,JSON_C_TO_STRING_PLAIN);
    fputs(json_str,stdout);
    fputc('\n',stdout);
    json_object_put(obj);
    fflush(stdout);
}

// 输出 400
void send_error_400(const char*message){
    _send_json_error(400,"Bad Request",message?message:"Invalid request");
}

// 输出 401
void send_error_401(const char*message){
    _send_json_error(401,"Unauthorized",message?message:"Authentication required");
}

// 输出 403
void send_error_403(const char*message){
    _send_json_error(403,"Forbidden",message?message:"Access denied");
}

// 输出 404
void send_error_404(const char*message){
    _send_json_error(404,"Not Found",message?message:"Endpoint not found");
}

// 输出 405
void send_error_405(const char*message){
    _send_json_error(405,"Method Not Allowed",message?message:"HTTP method not supported");
}

// 输出 500
void send_error_500(const char*message){
    _send_json_error(500,"Internal Server Error",message?message:"Internal error occurred");
}
