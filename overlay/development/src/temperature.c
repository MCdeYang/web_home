#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <pthread.h>
#include <json-c/json.h>
#include "temperature.h"


// 初始化串口
int init_serial(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        perror("Failed to open serial port");
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("tcgetattr failed");
        close(fd);
        return -1;
    }

    cfmakeraw(&options);
    cfsetspeed(&options, BAUD_RATE);

    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("tcsetattr failed");
        close(fd);
        return -1;
    }

    // 清空缓冲区
    tcflush(fd, TCIOFLUSH);
    return fd;
}

// 解析响应字符串，提取湿度和温度
int parse_response(const char *buf, float *humidity, float *temperature) {
    // 示例: "R:55.2RH 23.4C\r\n"
    if (sscanf(buf, "R:%fRH %fC", humidity, temperature) == 2) {
        return 0;
    }
    return -1;
}

// 写入 JSON 文件（带 flock 锁）
int write_json_to_file(float humidity, float temperature) {
    mkdir("/development/tmp", 0755);

    int fd = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open output file");
        return -1;
    }

    if (flock(fd, LOCK_EX) != 0) {
        perror("flock failed");
        close(fd);
        return -1;
    }

    // 格式化为一位小数的字符串
    char hum_str[32], temp_str[32];
    snprintf(hum_str, sizeof(hum_str), "%.1f", humidity);
    snprintf(temp_str, sizeof(temp_str), "%.1f", temperature);

    // 构建 JSON
    json_object *root = json_object_new_object();
    json_object_object_add(root, "humidity", json_object_new_double_s(humidity, hum_str));
    json_object_object_add(root, "temperature", json_object_new_double_s(temperature, temp_str));
    json_object_object_add(root, "timestamp", json_object_new_int64(time(NULL)));

    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    ssize_t written = write(fd, json_str, strlen(json_str));
    if (written < 0) {
        perror("write failed");
    }

    flock(fd, LOCK_UN);
    close(fd);
    json_object_put(root);

    return (written >= 0) ? 0 : -1;
}

// 温度采集线程主函数
void* temperature_thread(void *arg) {
    int serial_fd = init_serial(SERIAL_PORT);
    if (serial_fd < 0) {
        fprintf(stderr, "Serial init failed, temperature thread exiting.\n");
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    while (1) {
        // 发送命令
        if (write(serial_fd, CMD, CMD_LEN) != CMD_LEN) {
            perror("Failed to send command");
            sleep(INTERVAL_SEC);
            continue;
        }

        // 读取响应（最多等待 2 秒）
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(serial_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';

            float humidity, temperature;
            if (parse_response(buffer, &humidity, &temperature) == 0) {
                printf("[TEMP] Got H=%.1f%%, T=%.1f°C\n", humidity, temperature);
                if (write_json_to_file(humidity, temperature) != 0) {
                    fprintf(stderr, "Failed to write JSON file\n");
                }
            } else {
                fprintf(stderr, "[TEMP] Unrecognized response: '%s'\n", buffer);
            }
        } else {
            fprintf(stderr, "[TEMP] No response from sensor\n");
        }

        sleep(INTERVAL_SEC);
    }

    close(serial_fd);
    return NULL;
}