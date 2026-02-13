/**********************************************************************
 * @file control.c
 * @brief 设备控制相关辅助函数实现
 *
 * 本文件实现控制类接口所需的通用解析与校验函数
 * 供 CGI 处理函数解析请求并提取控制参数
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 主要提供对 JSON 请求体的解析能力
 **********************************************************************/
#include "control.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <json-c/json.h>

// 从 JSON 请求体解析 state 字段
int parse_state_from_json(const char*json_body){
    json_object*root;
    json_object*state_obj;
    int state;
    int64_t val;

    if(!json_body){
        return -1;
    }

    root=json_tokener_parse(json_body);
    if(!root){
        fprintf(stderr,"JSON parse error\n");
        return -1;
    }

    state=-1;
    state_obj=NULL;
    if(json_object_object_get_ex(root,"state",&state_obj)){
        if(json_object_is_type(state_obj,json_type_int)){
            val=json_object_get_int64(state_obj);
            if(val==0||val==1){
                state=(int)val;
            }
        }
    }

    json_object_put(root);
    return state;
}
