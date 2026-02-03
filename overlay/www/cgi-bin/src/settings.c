/**********************************************************************
 * @file settings.c
 * @brief 设置相关业务函数实现
 *
 * 本文件实现账号配置与内网穿透相关的业务处理辅助函数
 * 供 CGI 处理函数调用完成配置读写与状态查询
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 涉及文件读写时需保证路径可写并注意权限
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <json-c/json.h>
#include "settings.h"

// 从行中提取指定键的值
void extract_value(const char*line,const char*key,char*out,size_t out_size){
    size_t key_len;
    const char*val;
    size_t val_len;

    if(!line||!key||!out||out_size==0){
        return;
    }
    key_len=strlen(key);
    if(strncmp(line,key,key_len)==0&&line[key_len]==':'){
        val=line+key_len+1;
        val_len=strcspn(val,"\r\n");
        if(val_len>=out_size){
            val_len=out_size-1;
        }
        strncpy(out,val,val_len);
        out[val_len]='\0';
    }
}

// JSON 转义
void json_escape(const char*src,char*dst,size_t dst_size){
    size_t i;
    size_t j;

    if(!src||!dst||dst_size==0){
        return;
    }
    i=0;
    j=0;
    while(src[i]&&j+2<dst_size){
        if(src[i]=='"'||src[i]=='\\'){
            dst[j++]='\\';
        }
        dst[j++]=src[i++];
    }
    dst[j]='\0';
}

// 写入账号信息文件
int write_login_file(const char*username,const char*password){
    FILE*f;

    f=fopen(LOGIN_FILE,"w");
    if(!f){
        return -1;
    }
    fprintf(f,"username:%s\npassword:%s\n",username,password);
    fclose(f);
    return 0;
}

// 读取用户名
void read_username(char*username,size_t size){
    FILE*f;
    char line[256];

    if(!username||size==0){
        return;
    }
    f=fopen(LOGIN_FILE,"r");
    if(!f){
        strncpy(username,"admin",size-1);
        username[size-1]='\0';
        return;
    }
    if(fgets(line,sizeof(line),f)){
        extract_value(line,"username",username,size);
    }else{
        strncpy(username,"admin",size-1);
        username[size-1]='\0';
    }
    fclose(f);
    if(username[0]=='\0'){
        strncpy(username,"admin",size-1);
        username[size-1]='\0';
    }
}

// 检查内网穿透进程是否运行
int is_ngrok_running(void){
    const char*pid_file;
    FILE*f;
    pid_t pid;

    pid_file="/development/web_tunnel/ngrok.pid";
    f=fopen(pid_file,"r");
    if(!f){
        return 0;
    }
    if(fscanf(f,"%d",&pid)!=1){
        fclose(f);
        return 0;
    }
    fclose(f);
    if(kill(pid,0)==0){
        return 1;
    }
    unlink(pid_file);
    return 0;
}

// 从 JSON 请求体提取账号密码
void parse_json_credentials(const char*body,char*username,char*password,size_t max_len){
    json_object*obj;
    json_object*juser;
    json_object*jpass;
    const char*u;
    const char*p;

    if(username&&max_len>0){
        username[0]='\0';
    }
    if(password&&max_len>0){
        password[0]='\0';
    }
    if(!body||!username||!password||max_len==0){
        return;
    }

    obj=json_tokener_parse(body);
    if(!obj||!json_object_is_type(obj,json_type_object)){
        if(obj){
            json_object_put(obj);
        }
        return;
    }

    juser=NULL;
    jpass=NULL;
    if(json_object_object_get_ex(obj,"username",&juser)){
        u=json_object_get_string(juser);
        if(u){
            strncpy(username,u,max_len-1);
            username[max_len-1]='\0';
        }
    }
    if(json_object_object_get_ex(obj,"password",&jpass)){
        p=json_object_get_string(jpass);
        if(p){
            strncpy(password,p,max_len-1);
            password[max_len-1]='\0';
        }
    }
    json_object_put(obj);
}

// 加载保存的账号密码
int load_stored_credentials(char*stored_user,size_t user_size,char*stored_pass,size_t pass_size){
    FILE*f;
    char line[256];

    if(!stored_user||!stored_pass||user_size==0||pass_size==0){
        return -1;
    }
    stored_user[0]='\0';
    stored_pass[0]='\0';

    f=fopen(LOGIN_FILE,"r");
    if(!f){
        return -1;
    }

    while(fgets(line,sizeof(line),f)){
        line[strcspn(line,"\r\n")]='\0';
        if(strncmp(line,"username:",9)==0){
            strncpy(stored_user,line+9,user_size-1);
            stored_user[user_size-1]='\0';
        }else if(strncmp(line,"password:",9)==0){
            strncpy(stored_pass,line+9,pass_size-1);
            stored_pass[pass_size-1]='\0';
        }
    }
    fclose(f);
    if(stored_user[0]&&stored_pass[0]){
        return 0;
    }
    return -1;
}
