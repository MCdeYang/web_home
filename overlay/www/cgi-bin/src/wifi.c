/**********************************************************************
 * @file wifi.c
 * @brief WiFi 相关工具函数实现
 *
 * 本文件实现执行脚本与字符串安全校验等通用功能
 * 供 CGI 接口调用以控制 WiFi 开关
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 仅提供命令执行与参数校验等基础能力
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "wifi.h"

// 执行命令并读取一行输出
int run_command_capture(const char*cmd,char*output,size_t out_size){
    FILE*fp;
    char*nl;

    fp=popen(cmd,"r");
    if(!fp){
        return -1;
    }
    output[0]='\0';
    if(fgets(output,(int)out_size,fp)==NULL){
        pclose(fp);
        return -1;
    }
    nl=strchr(output,'\n');
    if(nl){
        *nl='\0';
    }
    pclose(fp);
    return 0;
}

// 校验字符串是否只包含安全字符
int is_safe_string(const char*str,int allow_space){
    const char*p;

    if(!str){
        return 0;
    }
    for(p=str;*p;p++){
        if((*p>='a'&&*p<='z')||
            (*p>='A'&&*p<='Z')||
            (*p>='0'&&*p<='9')||
            *p=='_'||*p=='-'||*p=='.'||*p=='@'){
            continue;
        }
        if(allow_space&&*p==' '){
            continue;
        }
        return 0;
    }
    return 1;
}
