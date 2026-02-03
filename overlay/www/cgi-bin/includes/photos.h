#ifndef PHOTOS_H
#define PHOTOS_H

#include <stdio.h>
#include "define.h"

int photos_is_safe_filename(const char*name);
void photos_send_json_response(const char*json_str);
char*photos_build_list_json(void);
char*photos_save_uploaded_file(const char*body,const char*content_type);
char*photos_extract_filename_from_json(const char*json_body);
int photos_delete_photo_file(const char*filename);
void photo_url_decode(const char*src,char*dst);

#endif
