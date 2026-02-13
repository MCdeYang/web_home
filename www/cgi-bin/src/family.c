/**********************************************************************
 * @file family.c
 * @brief 家庭模块业务函数实现
 *
 * 本文件实现家庭成员与任务数据的读写处理
 * 供 CGI 处理函数完成成员管理与任务管理
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 数据文件为 JSON 格式并存放于临时目录
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <json-c/json.h>
#include "family.h"

// 从 Cookie 中提取用户名
char*get_username_from_cookie(void){
    const char*cookie;
    char*p;
    char*end;
    int len;
    char*user;
    int last;

    cookie=getenv("HTTP_COOKIE");
    if(!cookie){
        return NULL;
    }

    p=strstr(cookie,"user=");
    if(!p){
        return NULL;
    }
    p+=5;

    end=strchr(p,';');
    len=end?(int)(end-p):(int)strlen(p);
    if(len<=0||len>64){
        return NULL;
    }

    user=malloc((size_t)len+1);
    if(!user){
        return NULL;
    }
    strncpy(user,p,(size_t)len);
    user[len]='\0';

    while(*user==' '){
        memmove(user,user+1,strlen(user));
    }
    last=(int)strlen(user)-1;
    while(last>=0&&user[last]==' '){
        user[last--]='\0';
    }
    return user;
}

// 加载家庭数据
json_object*load_family_data(void){
    FILE*fp;
    long size;
    char*buf;
    json_object*root;
    json_object*members;

    fp=fopen(FAMILY_DATA_PATH,"r");
    if(!fp){
        root=json_object_new_object();
        json_object_object_add(root,"members",json_object_new_array());
        return root;
    }

    fseek(fp,0,SEEK_END);
    size=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(size<=0){
        fclose(fp);
        root=json_object_new_object();
        json_object_object_add(root,"members",json_object_new_array());
        return root;
    }

    buf=malloc((size_t)size+1);
    if(!buf){
        fclose(fp);
        return NULL;
    }
    if(fread(buf,1,(size_t)size,fp)!=(size_t)size){
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[size]='\0';
    fclose(fp);

    root=json_tokener_parse(buf);
    free(buf);
    if(!root||!json_object_is_type(root,json_type_object)){
        if(root){
            json_object_put(root);
        }
        root=json_object_new_object();
        json_object_object_add(root,"members",json_object_new_array());
    }

    members=json_object_object_get(root,"members");
    if(!members||!json_object_is_type(members,json_type_array)){
        json_object_object_del(root,"members");
        json_object_object_add(root,"members",json_object_new_array());
    }
    return root;
}

// 保存家庭数据
int save_family_data(json_object*root){
    FILE*fp;
    const char*str;
    int ok;

    fp=fopen(FAMILY_DATA_PATH,"w");
    if(!fp){
        return -1;
    }
    str=json_object_to_json_string_ext(root,JSON_C_TO_STRING_PRETTY);
    ok=(fprintf(fp,"%s\n",str)>0);
    fclose(fp);
    if(ok){
        return 0;
    }
    return -1;
}

// 解析截止时间字符串
time_t parse_due_date(const char*due_str){
    struct tm tm;
    int y;
    int m;
    int d;
    int h;
    int min;

    if(!due_str){
        return (time_t)-1;
    }
    memset(&tm,0,sizeof(tm));
    if(sscanf(due_str,"%d-%d-%d %d:%d",&y,&m,&d,&h,&min)==5){
        tm.tm_year=y-1900;
        tm.tm_mon=m-1;
        tm.tm_mday=d;
        tm.tm_hour=h;
        tm.tm_min=min;
        tm.tm_isdst=-1;
        return mktime(&tm);
    }
    return (time_t)-1;
}
