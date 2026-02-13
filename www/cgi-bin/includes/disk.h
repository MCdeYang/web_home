#ifndef DISK_H
#define DISK_H

#include <sys/types.h>
#include <time.h>

typedef struct{
    char name[256];
    off_t size;
    int is_dir;
    time_t mtime;
}disk_file_t;

void url_decode(const char*src,char*dst);
int list_files_in_dir(const char*dir_path,disk_file_t*files,int max_files);
int delete_path_recursive(const char*path);
extern int safe_filename(const char*name);

#endif
