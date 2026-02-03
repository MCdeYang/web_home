/**********************************************************************
 * @file environment.c
 * @brief 天气数据采集线程实现
 *
 * 本文件实现了通过和风天气 API 定期获取当前天气信息，
 * 并将 JSON 响应缓存到本地文件的后台守护线程。
 * 使用 libcurl 发起 HTTPS 请求，使用 gzip 自动解压，
 * 并通过文件锁确保多进程/线程环境下的写入安全。
 *
 * @author 杨翊
 * @date 2026-01-23
 * @version 1.0
 *
 * @note
 * - API 密钥和城市 ID 已硬编码，根据实际情况修改。
 * - 缓存文件路径为 /development/tmp/weather.json。
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <curl/curl.h>
#include "environment.h"
#include "define.h"

//回调函数 固定签名要求
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);// 重新分配内存
    if (!ptr) {
        fprintf(stderr, "realloc() failed\n");
        return 0;
    }
    //更新内存指针
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';
    return realsize;
}

//辅助函数：获取当前时间字符串
static void get_current_time_str(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

//核心函数：获取天气数据并写入文件
static int fetch_weather_to_file() {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = {0};
    chunk.memory = malloc(1); //确保初始内存为1字节，避免realloc失败
    chunk.size = 0;
    char url[512];
    if (!chunk.memory) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }
    //使用snprintf构建URL
    snprintf(url, sizeof(url),
        "https://%s/v7/weather/now?location=%s&key=%s",
        API_HOST, CITY_ID, API_KEY
    );
    curl = curl_easy_init();// 初始化curl
    if (!curl) {
        free(chunk.memory);
        fprintf(stderr, "curl_easy_init failed\n");
        return -1;
    }
    // 设置curl选项
    curl_easy_setopt(curl, CURLOPT_URL, url);// 设置URL
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);// 设置写入回调函数
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);// 设置写入数据指针
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);// 设置超时时间
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip"); // 支持gzip自动解压
    // 执行curl请求
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return -1;
    }
    int fd = open(WEATHER_CACHE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "Failed to open weather cache file for writing\n");
    } 
    else {
        // 加文件锁确保写入安全
        if (flock(fd, LOCK_EX) != 0) {
            perror("flock failed");
        } else {
            ssize_t written = write(fd, chunk.memory, chunk.size);
            if (written < 0) {
                perror("write failed");
            }
            flock(fd, LOCK_UN);
        }
        close(fd);
    }
    curl_easy_cleanup(curl);// 清理curl
    free(chunk.memory);
    return 0;
}
// 环境数据采集线程
void *environment_thread(void *arg) {
    printf("environment daemon thread started.\n");
    printf("Weather cache file: %s\n", WEATHER_CACHE_FILE);
    printf("Checking weather every %d seconds.\n", WEATHER_INTERVAL);
    while (1) {
        char timebuf[64];
        get_current_time_str(timebuf, sizeof(timebuf));
        printf("[%s] Fetching weather...\n", timebuf);
        fetch_weather_to_file();
        sleep(WEATHER_INTERVAL);
    }
    return NULL;
}