/**********************************************************************
 * @file token.c
 * @brief 会话令牌管理实现
 *
 * 本文件实现令牌生成与校验及会话文件维护
 * 供 CGI 认证流程校验请求合法性
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 会话采用文件形式存储并按时间过期清理
 **********************************************************************/
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include "token.h"

#ifndef CLOCK_REALTIME
#define USE_FALLBACK_TIME 1
#else
#define USE_FALLBACK_TIME 0
#endif

// 确保会话目录存在
static void ensure_session_dir(void){
    mkdir(SESSION_DIR,0755);
    chmod(SESSION_DIR,0777);
}

// 计算混合哈希
static unsigned long mix_hash(unsigned long a,unsigned long b,unsigned long c){
    unsigned long hash;

    hash=5381;
    hash=((hash<<5)+hash)^a;
    hash=((hash<<5)+hash)^b;
    hash=((hash<<5)+hash)^c;
    return hash;
}

// 生成令牌字符串
void generate_token(char*token,size_t len){
    static unsigned int counter=0;
    unsigned long seed1;
    unsigned long seed2;
    unsigned long seed3;
    unsigned long h1;
    unsigned long h2;

    if(!token||len<(size_t)TOKEN_LEN+1){
        if(token){
            token[0]='\0';
        }
        return;
    }

    counter++;
    seed1=0;
    seed2=0;
    seed3=0;

#if USE_FALLBACK_TIME
    {
        time_t t;
        t=time(NULL);
        seed1=(unsigned long)t;
        seed2=(unsigned long)getpid();
        seed3=(unsigned long)counter;
    }
#else
    {
        struct timespec ts;
        if(clock_gettime(CLOCK_REALTIME,&ts)==0){
            seed1=(unsigned long)ts.tv_sec;
            seed2=(unsigned long)ts.tv_nsec;
            seed3=((unsigned long)getpid()<<16)|(counter&0xFFFF);
        }else{
            time_t t;
            t=time(NULL);
            seed1=(unsigned long)t;
            seed2=(unsigned long)getpid();
            seed3=(unsigned long)counter;
        }
    }
#endif

    h1=mix_hash(seed1,seed2,seed3);
    h2=mix_hash(seed2,seed3,seed1);

    snprintf(token,(size_t)TOKEN_LEN+1,"%08lx%08lx%08lx%08lx",
        h1&0xFFFFFFFFUL,
        (h1>>32)&0xFFFFFFFFUL,
        h2&0xFFFFFFFFUL,
        (h2>>32)&0xFFFFFFFFUL);
    token[TOKEN_LEN]='\0';
}

// 保存令牌
int add_token(const char*token){
    char path[256];
    FILE*f;

    if(!token||strlen(token)!=(size_t)TOKEN_LEN){
        return 0;
    }
    ensure_session_dir();
    snprintf(path,sizeof(path),"%s/%s",SESSION_DIR,token);
    f=fopen(path,"w");
    if(!f){
        return 0;
    }
    fprintf(f,"%ld",(long)time(NULL));
    fclose(f);
    chmod(path,0644);
    return 1;
}

// 校验令牌
int is_valid_token(const char*token){
    char path[256];
    FILE*f;
    long saved_time;
    time_t now;

    if(!token||strlen(token)!=(size_t)TOKEN_LEN){
        return 0;
    }
    snprintf(path,sizeof(path),"%s/%s",SESSION_DIR,token);
    f=fopen(path,"r");
    if(!f){
        return 0;
    }
    if(fscanf(f,"%ld",&saved_time)!=1){
        fclose(f);
        unlink(path);
        return 0;
    }
    fclose(f);
    now=time(NULL);
    if((long)(now-saved_time)>EXPIRE_SECONDS){
        unlink(path);
        return 0;
    }
    return 1;
}

// 从 Cookie 获取令牌
char*get_token_from_cookie(void){
    char*cookie;
    char*p;
    char*end;
    int len;
    char*out;

    cookie=getenv("HTTP_COOKIE");
    if(!cookie){
        return NULL;
    }
    p=strstr(cookie,"token=");
    if(!p){
        return NULL;
    }
    p+=6;
    end=strchr(p,';');
    len=end?(int)(end-p):(int)strlen(p);
    if(len!=TOKEN_LEN){
        return NULL;
    }
    out=malloc((size_t)TOKEN_LEN+1);
    if(!out){
        return NULL;
    }
    strncpy(out,p,(size_t)len);
    out[len]='\0';
    return out;
}

// 清理所有令牌
void clear_all_tokens(void){
    DIR*dir;
    struct dirent*entry;
    char path[PATH_MAX];
    int n;

    dir=opendir(SESSION_DIR);
    if(!dir){
        return;
    }
    while((entry=readdir(dir))!=NULL){
        if(strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0){
            continue;
        }
        n=snprintf(path,sizeof(path),"%s/%s",SESSION_DIR,entry->d_name);
        if(n<0||(size_t)n>=sizeof(path)){
            continue;
        }
        unlink(path);
    }
    closedir(dir);
    rmdir(SESSION_DIR);
}
