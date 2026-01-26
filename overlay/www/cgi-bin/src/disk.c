// disk.c
#define _GNU_SOURCE
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ftw.h>
#include <ctype.h>
#include "disk.h"

#ifndef DISK_ROOT
#define DISK_ROOT "/mnt/ssd"
#endif

const char* get_disk_root(void) {
    return DISK_ROOT;
}
/*
void url_decode(const char *src, char *dst) {
    if (!src || !dst) return;
    while (*src) {
        if (*src == '%' && src[1] && src[2] &&
            isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}
*/
void url_decode(const char *src, char *dst) {
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}
/*
int is_safe_relative_path(const char *path) {
    if (!path || path[0] != '/') return 0;
    if (strstr(path, "..") != NULL) return 0;
    if (strchr(path, '\\') != NULL) return 0;
    if (strlen(path) > 512) return 0;
    if (strlen(path) > 1 && path[strlen(path)-1] == '/') return 0;
    return 1;
}
    */

int list_files_in_dir(const char *dir_path, disk_file_t *files, int max_files) {
    DIR *dir = opendir(dir_path);
    if (!dir) return -1;

    struct dirent *entry;
    int count = 0;
    struct stat st;
    char full_path[1024];

    while ((entry = readdir(dir)) != NULL && count < max_files) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        if (stat(full_path, &st) != 0) continue;

        strncpy(files[count].name, entry->d_name, sizeof(files[count].name) - 1);
        files[count].name[sizeof(files[count].name) - 1] = '\0';
        files[count].size = S_ISDIR(st.st_mode) ? 0 : st.st_size;
        files[count].is_dir = S_ISDIR(st.st_mode);
        files[count].mtime = st.st_mtime;
        count++;
    }

    closedir(dir);
    return count;
}

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    return remove(fpath);
}

int delete_path_recursive(const char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}