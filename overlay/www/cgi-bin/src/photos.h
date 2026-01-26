#ifndef PHOTOS_H
#define PHOTOS_H

#include <stdio.h>
#define PHOTOS_DIR "/media/sdcard/photos"
// 安全文件名检查
int photos_is_safe_filename(const char *name);

// 发送 JSON 响应（CGI 模式）
void photos_send_json_response(const char *json_str);

// 构建照片列表 JSON 字符串（调用者需 free）
char* photos_build_list_json(void);

// 保存上传的文件（multipart body + content-type → 保存到磁盘）
// 成功返回 filename，失败返回 NULL（需 free 返回值）
char* photos_save_uploaded_file(const char *body, const char *content_type);


char* photos_extract_filename_from_json(const char *json_body);

// 安全删除照片文件
// 成功返回 1，失败返回 0
int photos_delete_photo_file(const char *filename);
void photo_url_decode(const char *src, char *dst);
#endif