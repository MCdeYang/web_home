/**********************************************************************
 * @file disk.c
 * @brief 磁盘文件操作模块实现
 *
 * 本文件实现磁盘目录遍历与文件删除等辅助能力
 * 供 CGI 处理函数完成上传下载与目录管理
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 访问路径需校验避免越权访问
 **********************************************************************/
#define _GNU_SOURCE
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include "disk.h"
#include "define.h"

// 获取磁盘根目录
const char*get_disk_root(void){
    return DISK_ROOT;
}
// URL 解码
void url_decode(const char*src,char*dst){
    char hex[3];

    if(!src||!dst){
        return;
    }
    hex[2]='\0';
    while(*src){
        if(*src=='%'&&src[1]&&src[2]){
            hex[0]=src[1];
            hex[1]=src[2];
            *dst++=(char)strtol(hex,NULL,16);
            src+=3;
            continue;
        }
        if(*src=='+'){
            *dst++=' ';
            src++;
            continue;
        }
        *dst++=*src++;
    }
    *dst='\0';
}

// 列出目录文件
int list_files_in_dir(const char*dir_path,disk_file_t*files,int max_files){
    DIR*dir;
    struct dirent*entry;
    int count;
    struct stat st;
    char full_path[1024];

    if(!dir_path||!files||max_files<=0){
        return -1;
    }
    dir=opendir(dir_path);
    if(!dir){
        return -1;
    }

    count=0;
    while((entry=readdir(dir))!=NULL&&count<max_files){
        if(strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0){
            continue;
        }
        snprintf(full_path,sizeof(full_path),"%s/%s",dir_path,entry->d_name);
        if(stat(full_path,&st)!=0){
            continue;
        }
        strncpy(files[count].name,entry->d_name,sizeof(files[count].name)-1);
        files[count].name[sizeof(files[count].name)-1]='\0';
        files[count].size=S_ISDIR(st.st_mode)?0:st.st_size;
        files[count].is_dir=S_ISDIR(st.st_mode)?1:0;
        files[count].mtime=st.st_mtime;
        count++;
    }
    closedir(dir);
    return count;
}

// 递归删除回调
static int unlink_cb(const char*fpath,const struct stat*sb,int typeflag,struct FTW*ftwbuf){
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    return remove(fpath);
}

// 递归删除路径
int delete_path_recursive(const char*path){
    return nftw(path,unlink_cb,64,FTW_DEPTH|FTW_PHYS);
}
