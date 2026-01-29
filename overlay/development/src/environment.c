#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <curl/curl.h>
#include "environment.h"

#define WEATHER_CACHE_FILE "/development/tmp/weather.json"
#define CITY_ID "101060501"          // 吉林省通化市
#define API_KEY "8464c2a756f64b2d81da04e28529fc53"
#define API_HOST "nb3yfrku57.re.qweatherapi.com"
#define WEATHER_INTERVAL 600   // 10分钟

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "realloc() failed\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

void get_current_time_str(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

int fetch_weather_to_file() {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = {0};
    chunk.memory = malloc(1);
    chunk.size = 0;

    if (!chunk.memory) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    char url[512];
    snprintf(url, sizeof(url),
        "https://%s/v7/weather/now?location=%s&key=%s",
        API_HOST, CITY_ID, API_KEY
    );

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        fprintf(stderr, "curl_easy_init failed\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip"); // 支持gzip自动解压

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
    } else {
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

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

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