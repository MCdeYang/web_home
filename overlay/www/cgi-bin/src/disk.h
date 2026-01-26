// disk.h
#ifndef DISK_H
#define DISK_H

#include <sys/types.h>

typedef struct {
    char name[256];
    off_t size;
    int is_dir;
    time_t mtime;
} disk_file_t;

// ===== 辅助函数（由 disk.c 实现）=====
void url_decode(const char *src, char *dst);
//int is_safe_relative_path(const char *path);
int list_files_in_dir(const char *dir_path, disk_file_t *files, int max_files);
int delete_path_recursive(const char *path);

// 安全文件名校验（假设你在 token.c 或 system.c 已有，若没有可放这里）
extern int safe_filename(const char *name);

#endif