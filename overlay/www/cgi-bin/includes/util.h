#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <sys/types.h>
#include <json-c/json.h>
#include "define.h"

int parse_multipart_and_save(const char*body,long content_length,const char*boundary,const char*target_dir);
int make_parent_writable(const char*dirpath);
int mkpath(const char*dir,mode_t mode);
int extract_filename_from_header(const char*header,char*out_filename,size_t out_size);
int is_safe_filename(const char*name);
int is_safe_relative_path(const char*path);
int parse_rename_json(const char*body,char*path,char*old_name,char*new_name);
int parse_query_string(const char*query,const char*key,char*out,size_t out_len);
int extract_json_string(json_object*obj,const char*key,char*out,size_t len);

#endif
