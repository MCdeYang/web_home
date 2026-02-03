/**********************************************************************
 * @file photos.c
 * @brief 照片管理模块实现
 *
 * 本文件实现照片文件的安全校验与读写等功能
 * 供 CGI 处理函数完成照片上传列表与删除等操作
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 操作文件前需校验文件名避免路径穿越
 **********************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include "photos.h"

// 校验照片文件名
int photos_is_safe_filename(const char*name){
    size_t n;
    const char*p;
    const char*ext;

    if(!name){
        return 0;
    }
    n=strlen(name);
    if(n==0||n>250){
        return 0;
    }
    if(name[0]=='.'){
        return 0;
    }

    for(p=name;*p;p++){
        if(*p=='/'||*p=='\\'||*p==':'||*p=='*'||*p=='?'||*p=='"'||*p=='<'||*p=='>'||*p=='|'){
            return 0;
        }
    }

    ext=strrchr(name,'.');
    if(!ext||ext==name){
        return 0;
    }
    ext++;
    if(strcasecmp(ext,"jpg")==0||
        strcasecmp(ext,"jpeg")==0||
        strcasecmp(ext,"png")==0||
        strcasecmp(ext,"gif")==0){
        return 1;
    }
    return 0;
}

// 输出 JSON 响应
void photos_send_json_response(const char*json_str){
    size_t len;

    if(!json_str){
        return;
    }
    len=strlen(json_str);
    printf("Content-Type: application/json\r\n");
    printf("Content-Length: %zu\r\n",len);
    printf("\r\n");
    printf("%s", json_str);
    fflush(stdout);
}

// 构建照片列表 JSON
char*photos_build_list_json(void){
    DIR*dir;
    char*json;
    char*err;
    int first;
    struct dirent*entry;
    char escaped[260];

    dir=opendir(PHOTOS_DIR);
    if(!dir){
        err=malloc(64);
        if(!err){
            return NULL;
        }
        strcpy(err,"{\"error\":\"open dir failed\"}");
        return err;
    }

    json=malloc(8192);
    if(!json){
        closedir(dir);
        err=malloc(64);
        if(!err){
            return NULL;
        }
        strcpy(err,"{\"error\":\"malloc failed\"}");
        return err;
    }

    strcpy(json,"[");
    first=1;
    entry=NULL;
    while((entry=readdir(dir))!=NULL){
        if(entry->d_type==DT_REG&&photos_is_safe_filename(entry->d_name)){
            if(!first){
                strcat(json,",");
            }
            snprintf(escaped,sizeof(escaped),"\"%s\"",entry->d_name);
            strcat(json,escaped);
            first=0;
        }
    }
    closedir(dir);
    strcat(json,"]");
    return json;
}

// 解析上传数据并保存文件
char*photos_save_uploaded_file(const char*body,const char*content_type){
    const char*boundary_start;
    const char*bnd_end;
    char raw_boundary[256];
    const char*file_start;
    const char*fname_start;
    const char*fname_end;
    size_t fname_len;
    char*filename;
    const char*data_start;
    const char*data_end;
    size_t body_len;
    char end_marker1[300];
    char end_marker2[300];
    char mid_marker[300];
    int len1;
    int len2;
    int len_mid;
    size_t file_size;
    char filepath[512];
    FILE*fp;

    if(!body||!content_type){
        return NULL;
    }

    boundary_start=strstr(content_type,"boundary=");
    if(!boundary_start){
        return NULL;
    }
    boundary_start+=9;
    while(*boundary_start==' '){
        boundary_start++;
    }

    raw_boundary[0]='\0';
    bnd_end=NULL;
    if(*boundary_start=='"'){
        boundary_start++;
        bnd_end=strchr(boundary_start,'"');
    }else{
        bnd_end=strchr(boundary_start,';');
        if(!bnd_end){
            bnd_end=boundary_start+strlen(boundary_start);
        }
    }
    if(!bnd_end||(size_t)(bnd_end-boundary_start)>=sizeof(raw_boundary)-1){
        return NULL;
    }
    memcpy(raw_boundary,boundary_start,(size_t)(bnd_end-boundary_start));
    raw_boundary[bnd_end-boundary_start]='\0';

    file_start=strstr(body,"filename=\"");
    if(!file_start){
        return NULL;
    }
    fname_start=file_start+10;
    fname_end=strchr(fname_start,'"');
    if(!fname_end){
        return NULL;
    }
    fname_len=(size_t)(fname_end-fname_start);
    if(fname_len==0||fname_len>250){
        return NULL;
    }

    filename=malloc(fname_len+1);
    if(!filename){
        return NULL;
    }
    memcpy(filename,fname_start,fname_len);
    filename[fname_len]='\0';
    if(!photos_is_safe_filename(filename)){
        free(filename);
        return NULL;
    }

    data_start=strstr(fname_end,"\r\n\r\n");
    if(!data_start){
        free(filename);
        return NULL;
    }
    data_start+=4;

    data_end=NULL;
    body_len=strlen(body);
    len1=snprintf(end_marker1,sizeof(end_marker1),"\r\n--%s--\r\n",raw_boundary);
    if(len1>0&&(size_t)len1<sizeof(end_marker1)){
        data_end=(const char*)memmem(data_start,(size_t)(body+body_len-data_start),end_marker1,(size_t)len1);
    }
    if(!data_end){
        len2=snprintf(end_marker2,sizeof(end_marker2),"\r\n--%s--",raw_boundary);
        if(len2>0&&(size_t)len2<sizeof(end_marker2)){
            data_end=(const char*)memmem(data_start,(size_t)(body+body_len-data_start),end_marker2,(size_t)len2);
        }
    }
    if(!data_end){
        len_mid=snprintf(mid_marker,sizeof(mid_marker),"\r\n--%s\r\n",raw_boundary);
        if(len_mid>0&&(size_t)len_mid<sizeof(mid_marker)){
            data_end=(const char*)memmem(data_start,(size_t)(body+body_len-data_start),mid_marker,(size_t)len_mid);
        }
    }
    if(!data_end){
        data_end=body+body_len;
    }

    file_size=(size_t)(data_end-data_start);
    if(file_size==0){
        free(filename);
        return NULL;
    }

    if((size_t)snprintf(filepath,sizeof(filepath),"%s/%s",PHOTOS_DIR,filename)>=sizeof(filepath)){
        free(filename);
        return NULL;
    }
    fp=fopen(filepath,"wb");
    if(!fp){
        free(filename);
        return NULL;
    }
    if(fwrite(data_start,1,file_size,fp)!=file_size){
        fclose(fp);
        unlink(filepath);
        free(filename);
        return NULL;
    }
    fclose(fp);
    return filename;
}

// 从 JSON 请求体提取 filename
char*photos_extract_filename_from_json(const char*json_body){
    json_object*root;
    json_object*fname_obj;
    const char*fname_str;
    char*result;

    if(!json_body){
        return NULL;
    }
    root=json_tokener_parse(json_body);
    if(!root){
        return NULL;
    }
    fname_obj=NULL;
    if(!json_object_object_get_ex(root,"filename",&fname_obj)){
        json_object_put(root);
        return NULL;
    }
    if(json_object_get_type(fname_obj)!=json_type_string){
        json_object_put(root);
        return NULL;
    }
    fname_str=json_object_get_string(fname_obj);
    if(!fname_str||strlen(fname_str)==0){
        json_object_put(root);
        return NULL;
    }
    result=strdup(fname_str);
    json_object_put(root);
    return result;
}

// 删除照片文件
int photos_delete_photo_file(const char*filename){
    char filepath[512];

    if(!filename){
        return 0;
    }
    if(!photos_is_safe_filename(filename)){
        return 0;
    }
    snprintf(filepath,sizeof(filepath),"%s/%s",PHOTOS_DIR,filename);
    if(unlink(filepath)==0){
        return 1;
    }
    return 0;
}
// 将十六进制字符转为数值
static unsigned char hex_char_to_byte(char c){
    if(c>='0'&&c<='9'){
        return (unsigned char)(c-'0');
    }
    if(c>='A'&&c<='F'){
        return (unsigned char)(c-'A'+10);
    }
    if(c>='a'&&c<='f'){
        return (unsigned char)(c-'a'+10);
    }
    return 0;
}

// URL 解码
void photo_url_decode(const char*src,char*dst){
    unsigned char high;
    unsigned char low;

    if(!src||!dst){
        return;
    }
    while(*src){
        if(*src=='%'&&src[1]&&src[2]){
            high=hex_char_to_byte(src[1]);
            low=hex_char_to_byte(src[2]);
            *dst++=(char)((high<<4)|low);
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
