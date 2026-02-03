/**********************************************************************
 * @file util.c
 * @brief CGI 通用工具函数实现
 *
 * 本文件实现 multipart 解析与路径校验等通用工具
 * 供多个 CGI 处理函数复用以减少重复代码
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 对外输入需严格校验避免路径穿越与注入风险
 **********************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <json-c/json.h>
#include "util.h"

// URL 解码到输出缓冲区
static void url_decode_to(const char*src,char*dst,size_t dst_size){
    char*o;
    unsigned int v;

    if(!dst||dst_size==0){
        return;
    }
    if(!src){
        dst[0]='\0';
        return;
    }

    o=dst;
    while(*src&&dst_size>1){
        if(*src=='%'&&isxdigit((unsigned char)src[1])&&isxdigit((unsigned char)src[2])){
            v=0;
            if(src[1]>='0'&&src[1]<='9')v=(unsigned int)(src[1]-'0')<<4;
            else if(src[1]>='a'&&src[1]<='f')v=(unsigned int)(src[1]-'a'+10)<<4;
            else if(src[1]>='A'&&src[1]<='F')v=(unsigned int)(src[1]-'A'+10)<<4;

            if(src[2]>='0'&&src[2]<='9')v|=(unsigned int)(src[2]-'0');
            else if(src[2]>='a'&&src[2]<='f')v|=(unsigned int)(src[2]-'a'+10);
            else if(src[2]>='A'&&src[2]<='F')v|=(unsigned int)(src[2]-'A'+10);

            *o++=(char)v;
            src+=3;
            dst_size--;
            continue;
        }
        if(*src=='+'){
            *o++=' ';
            src++;
            dst_size--;
            continue;
        }
        *o++=*src++;
        dst_size--;
    }
    *o='\0';
}

// 解析 multipart 并保存文件到目标目录
int parse_multipart_and_save(const char*body,long content_length,const char*boundary,const char*target_dir){
    char full_boundary[300];
    const char*current;
    const char*headers_end;
    int is_file_part;
    const char*line_start;
    const char*line_end;
    size_t len;
    char filename[MAX_FILENAME];
    char header_line[1024];
    const char*data_start;
    const char*next_boundary;
    size_t data_len;
    char final_path[MAX_PATH];
    struct stat st;
    FILE*fp;

    if(!body||!boundary||!target_dir){
        return -1;
    }

    filename[0]='\0';
    snprintf(full_boundary,sizeof(full_boundary),"--%s",boundary);

    current=body;
    while((current=strstr(current,full_boundary))!=NULL){
        current+=strlen(full_boundary);
        if(strncmp(current,"--",2)==0){
            break;
        }
        headers_end=strstr(current,"\r\n\r\n");
        if(!headers_end){
            current+=1;
            continue;
        }

        is_file_part=0;
        line_start=current;
        filename[0]='\0';
        while(line_start<headers_end){
            line_end=strstr(line_start,"\r\n");
            if(!line_end||line_end>headers_end){
                break;
            }
            len=(size_t)(line_end-line_start);
            if(len>=sizeof(header_line)){
                len=sizeof(header_line)-1;
            }
            strncpy(header_line,line_start,len);
            header_line[len]='\0';

            if(strncasecmp(header_line,"Content-Disposition:",20)==0){
                if(strstr(header_line,"filename=")!=NULL){
                    if(extract_filename_from_header(header_line,filename,sizeof(filename))){
                        if(!is_safe_filename(filename)){
                            return -2;
                        }
                        is_file_part=1;
                    }
                    break;
                }
            }
            line_start=line_end+2;
        }

        if(!is_file_part){
            current=headers_end+4;
            continue;
        }

        data_start=headers_end+4;
        next_boundary=strstr(data_start,full_boundary);
        if(!next_boundary){
            next_boundary=body+content_length;
        }
        data_len=(size_t)(next_boundary-data_start);
        if(data_len==0){
            return -1;
        }

        if(stat(target_dir,&st)!=0||!S_ISDIR(st.st_mode)){
            return -3;
        }

        snprintf(final_path,sizeof(final_path),"%s/%s",target_dir,filename);
        fp=fopen(final_path,"wb");
        if(!fp){
            return -3;
        }
        if(fwrite(data_start,1,data_len,fp)!=data_len){
            fclose(fp);
            unlink(final_path);
            return -3;
        }
        fclose(fp);
        return 0;
    }

    return -1;
}

// 确保目录可写
int make_parent_writable(const char*dirpath){
    struct stat st;

    if(stat(dirpath,&st)==0){
        if(!S_ISDIR(st.st_mode)){
            return -1;
        }
        if((st.st_mode&0777)!=0777){
            if(chmod(dirpath,0777)!=0){
                return -1;
            }
        }
        return 0;
    }
    return -1;
}

// 递归创建目录
int mkpath(const char*dir,mode_t mode){
    char tmp[1024];
    char*p;
    size_t len;

    if(!dir){
        return -1;
    }
    if(strlen(dir)>=sizeof(tmp)){
        return -1;
    }

    snprintf(tmp,sizeof(tmp),"%s",dir);
    len=strlen(tmp);
    if(len>0&&tmp[len-1]=='/'){
        tmp[len-1]='\0';
    }

    for(p=tmp+1;*p;p++){
        if(*p=='/'){
            *p='\0';
            if(mkdir(tmp,mode)!=0&&errno!=EEXIST){
                *p='/';
                return -1;
            }
            *p='/';
        }
    }
    if(mkdir(tmp,mode)!=0&&errno!=EEXIST){
        return -1;
    }
    return 0;
}
// 从请求头中提取文件名
int extract_filename_from_header(const char*header,char*out_filename,size_t out_size){
    const char*p;
    const char*start;
    const char*end;
    size_t cplen;
    char decoded[512];
    char temp[512];

    if(!header||!out_filename||out_size==0){
        return 0;
    }
    out_filename[0]='\0';

    p=strstr(header,"filename*=");
    if(p){
        p+=10;
        while(*p==' '){
            p++;
        }
        if(strncmp(p,"UTF-8''",7)==0){
            p+=7;
            url_decode_to(p,decoded,sizeof(decoded));
            if(decoded[0]!='\0'){
                strncpy(out_filename,decoded,out_size-1);
                out_filename[out_size-1]='\0';
                return 1;
            }
        }
    }

    p=strstr(header,"filename=");
    if(p){
        p+=9;
        while(*p==' '){
            p++;
        }

        start=p;
        end=NULL;
        if(*p=='"'){
            start=p+1;
            end=start;
            while(*end){
                if(*end=='"'&&*(end-1)!='\\'){
                    break;
                }
                end++;
            }
            if(*end!='"'){
                end=NULL;
            }
        }else{
            end=strchr(start,';');
            if(!end)end=strchr(start,'\r');
            if(!end)end=start+strlen(start);
        }

        if(end&&end>start){
            cplen=(size_t)(end-start);
            if(cplen>=sizeof(temp)){
                cplen=sizeof(temp)-1;
            }
            strncpy(temp,start,cplen);
            temp[cplen]='\0';
            url_decode_to(temp,out_filename,out_size);
            return 1;
        }
    }

    return 0;
}

// 校验文件名是否安全
int is_safe_filename(const char*name){
    if(!name||!*name){
        return 0;
    }
    if(strstr(name,"..")||strchr(name,'/')||strchr(name,'\\')){
        return 0;
    }
    return 1;
}

// 校验相对路径是否安全
int is_safe_relative_path(const char*path){
    if(!path||!*path){
        return 0;
    }
    if(strstr(path,"..")||strstr(path,"//")){
        return 0;
    }
    return 1;
}
// 从简单 JSON 字符串提取字段值
static const char*extract_value(const char*json,const char*key,char*out,size_t out_size){
    char key_str[256];
    const char*p;
    const char*end;
    size_t vlen;

    if(!json||!key||!out||out_size==0){
        return NULL;
    }

    snprintf(key_str,sizeof(key_str),"\"%s\":",key);
    p=strstr(json,key_str);
    if(!p){
        return NULL;
    }
    p+=strlen(key_str);
    while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r'){
        p++;
    }
    if(*p!='"'){
        return NULL;
    }
    p++;
    end=strchr(p,'"');
    if(!end){
        return NULL;
    }
    vlen=(size_t)(end-p);
    if(vlen>=out_size){
        return NULL;
    }
    memcpy(out,p,vlen);
    out[vlen]='\0';
    return out;
}

// 解析重命名 JSON
int parse_rename_json(const char*body,char*path,char*old_name,char*new_name){
    if(extract_value(body,"path",path,1024)&&
        extract_value(body,"old_name",old_name,512)&&
        extract_value(body,"new_name",new_name,512)){
        return 1;
    }
    return 0;
}
// 从查询字符串提取指定键的值
int parse_query_string(const char*query,const char*key,char*out,size_t out_len){
    size_t key_len;
    const char*p;
    const char*eq;
    const char*value_start;
    const char*next_amp;
    size_t value_len;
    char decoded[1024];

    if(!query||!key||!out||out_len==0){
        return -1;
    }
    key_len=strlen(key);
    if(key_len==0){
        return -1;
    }

    p=query;
    while(*p){
        if(*p=='&'){
            p++;
            continue;
        }
        eq=strchr(p,'=');
        if(!eq){
            break;
        }
        if((size_t)(eq-p)==key_len&&strncmp(p,key,key_len)==0){
            value_start=eq+1;
            next_amp=strchr(value_start,'&');
            value_len=next_amp?(size_t)(next_amp-value_start):strlen(value_start);
            if(value_len>=sizeof(decoded)){
                value_len=sizeof(decoded)-1;
            }
            memcpy(decoded,value_start,value_len);
            decoded[value_len]='\0';
            url_decode_to(decoded,out,out_len);
            return 0;
        }
        p=strchr(eq,'&');
        if(!p){
            break;
        }
    }
    return -1;
}
// 从 JSON 对象提取字符串字段
int extract_json_string(json_object*obj,const char*key,char*out,size_t len){
    json_object*val;
    const char*str;
    size_t str_len;

    if(!obj||!key||!out||len==0){
        return -1;
    }
    if(!json_object_is_type(obj,json_type_object)){
        return -1;
    }
    val=NULL;
    if(!json_object_object_get_ex(obj,key,&val)){
        return -1;
    }
    if(!json_object_is_type(val,json_type_string)){
        return -1;
    }
    str=json_object_get_string(val);
    if(!str){
        out[0]='\0';
        return -1;
    }
    str_len=strlen(str);
    if(str_len>=len){
        str_len=len-1;
    }
    memcpy(out,str,str_len);
    out[str_len]='\0';
    return 0;
}
