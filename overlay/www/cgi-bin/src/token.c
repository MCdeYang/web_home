// token.c (多设备兼容版)
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

#define SESSION_DIR "/tmp/sessions"
#define EXPIRE_SECONDS 1800
#define TOKEN_LEN 32

#ifndef CLOCK_REALTIME
#define USE_FALLBACK_TIME 1
#else
#define USE_FALLBACK_TIME 0
#endif

static void ensure_session_dir(void) {
    mkdir(SESSION_DIR, 0755);
    // 确保 nobody 能写（重要！）
    chmod(SESSION_DIR, 0777);
}

static unsigned long mix_hash(unsigned long a, unsigned long b, unsigned long c) {
    unsigned long hash = 5381;
    hash = ((hash << 5) + hash) ^ a;
    hash = ((hash << 5) + hash) ^ b;
    hash = ((hash << 5) + hash) ^ c;
    return hash;
}

void generate_token(char *token, size_t len) {
    if (!token || len < TOKEN_LEN + 1) {
        if (token) token[0] = '\0';
        return;
    }

    static unsigned int counter = 0;
    counter++;

    unsigned long seed1 = 0, seed2 = 0, seed3 = 0;

#if USE_FALLBACK_TIME
    time_t t = time(NULL);
    seed1 = (unsigned long)t;
    seed2 = (unsigned long)getpid();
    seed3 = (unsigned long)counter;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        seed1 = (unsigned long)ts.tv_sec;
        seed2 = (unsigned long)ts.tv_nsec;
        seed3 = ((unsigned long)getpid() << 16) | (counter & 0xFFFF);
    } else {
        time_t t = time(NULL);
        seed1 = (unsigned long)t;
        seed2 = (unsigned long)getpid();
        seed3 = (unsigned long)counter;
    }
#endif

    unsigned long h1 = mix_hash(seed1, seed2, seed3);
    unsigned long h2 = mix_hash(seed2, seed3, seed1);

    snprintf(token, TOKEN_LEN + 1,
         "%08lx%08lx%08lx%08lx",
         h1 & 0xFFFFFFFFUL,
         (h1 >> 32) & 0xFFFFFFFFUL,
         h2 & 0xFFFFFFFFUL,
         (h2 >> 32) & 0xFFFFFFFFUL);
    token[TOKEN_LEN] = '\0';
}

int add_token(const char *token) {
    if (!token || strlen(token) != TOKEN_LEN) {
        return 0;
    }

    ensure_session_dir();

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, token);

    FILE *f = fopen(path, "w");
    if (!f) {
        return 0;
    }

    fprintf(f, "%ld", (long)time(NULL));
    fclose(f);
    chmod(path, 0644); // 允许 nobody 读写
    return 1;
}

int is_valid_token(const char *token) {
    if (!token || strlen(token) != TOKEN_LEN) {
        return 0;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, token);

    FILE *f = fopen(path, "r");
    if (!f) {
        return 0; // 文件不存在 = token 无效
    }

    long saved_time;
    if (fscanf(f, "%ld", &saved_time) != 1) {
        fclose(f);
        unlink(path);
        return 0;
    }
    fclose(f);

    // 检查是否过期
    if (time(NULL) - saved_time > EXPIRE_SECONDS) {
        unlink(path);
        return 0;
    }

    return 1;
}

char* get_token_from_cookie(void) {
    char *cookie = getenv("HTTP_COOKIE");
    if (!cookie) return NULL;

    char *p = strstr(cookie, "token=");
    if (!p) return NULL;

    p += 6;
    char *end = strchr(p, ';');
    int len = end ? (int)(end - p) : (int)strlen(p);

    if (len != TOKEN_LEN) return NULL;

    char *token = malloc(TOKEN_LEN + 1);
    if (!token) return NULL;
    strncpy(token, p, len);
    token[len] = '\0';
    return token;
}

void clear_all_tokens(void) {
    DIR *dir = opendir(SESSION_DIR);
    if (!dir) return;

    struct dirent *entry;
    char path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, entry->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "[WARN] Skipping too-long session file: %s\n", entry->d_name);
            continue;
        }
        unlink(path);
    }
    closedir(dir);
    rmdir(SESSION_DIR);
}