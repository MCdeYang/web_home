#include "zigbee_mq.h"  // 包含 ZIGBEE_CMD_XXX 和 send_zigbee_command
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <termios.h>
#include <stdint.h>
#include <sys/file.h>     // for flock
#include <json-c/json.h>  // json-c 头文件

#include "voice.h"
// 读取温度传感器数据
static int read_temperature_data(float* temperature, float* humidity) {
    const char* filepath = "/development/tmp/temperature.json";
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        return -1;
    }

    // 加共享锁
    if (flock(fileno(fp), LOCK_SH) != 0) {
        fclose(fp);
        return -2;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len <= 0 || len > 4096) {
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
        return -3;
    }
    rewind(fp);

    // 读取文件内容
    char* json_str = malloc(len + 1);
    if (!json_str) {
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
        return -4;
    }

    if (fread(json_str, 1, len, fp) != (size_t)len) {
        free(json_str);
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
        return -5;
    }
    json_str[len] = '\0';

    flock(fileno(fp), LOCK_UN);
    fclose(fp);

    // 解析 JSON
    json_object* root = json_tokener_parse(json_str);
    free(json_str);

    if (!root) {
        return -6;
    }

    // 提取温度和湿度字段
    json_object* temp_obj = NULL;
    json_object* hum_obj = NULL;

    if (!json_object_object_get_ex(root, "temperature", &temp_obj) ||
        !json_object_object_get_ex(root, "humidity", &hum_obj)) {
        json_object_put(root);
        return -7;
    }

    *temperature = (float)json_object_get_double(temp_obj);
    *humidity = (float)json_object_get_double(hum_obj);

    json_object_put(root);
    return 0;
}

// 发送温度数据
static int send_temperature_data(int fd) {
    float temperature, humidity;
    if (read_temperature_data(&temperature, &humidity) != 0) {
        return -1;
    }

    // 直接转换为整数（不乘以10）
    int32_t temp_int = (int32_t)(temperature);
    
    uint8_t frame[9] = {
        0xAA, 0x55, 0x02, 
        (uint8_t)(temp_int & 0xFF),
        (uint8_t)((temp_int >> 8) & 0xFF),
        (uint8_t)((temp_int >> 16) & 0xFF),
        (uint8_t)((temp_int >> 24) & 0xFF),
        0x55, 0xAA
    };

    ssize_t sent = write(fd, frame, sizeof(frame));
    if (sent != sizeof(frame)) {
        return -1;
    }

    printf("[Voice] Sent temperature: %d°C\n", (int)temp_int);
    return 0;
}

// 发送湿度数据
static int send_humidity_data(int fd) {
    float temperature, humidity;
    if (read_temperature_data(&temperature, &humidity) != 0) {
        return -1;
    }

    // 直接转换为整数（不乘以10）
    int32_t hum_int = (int32_t)(humidity);
    
    uint8_t frame[9] = {
        0xAA, 0x55, 0x03,
        (uint8_t)(hum_int & 0xFF),
        (uint8_t)((hum_int >> 8) & 0xFF),
        (uint8_t)((hum_int >> 16) & 0xFF),
        (uint8_t)((hum_int >> 24) & 0xFF),
        0x55, 0xAA
    };

    ssize_t sent = write(fd, frame, sizeof(frame));
    if (sent != sizeof(frame)) {
        return -1;
    }

    printf("[Voice] Sent humidity: %d%%\n", (int)hum_int);
    return 0;
}
// 获取系统IP地址
static int get_local_ip_address(uint8_t ip_bytes[4]) {
    int sockfd;
    struct ifreq ifr;
    
    // 创建socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    // 设置接口名称（通常使用eth0或wlan0，这里尝试常见的接口）
    const char* interfaces[] = {"eth0", "wlan0", "enp0s3", "wlp0s20f3"};
    int found = 0;
    
    for (int i = 0; i < 4 && !found; i++) {
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, interfaces[i], IFNAMSIZ - 1);
        
        if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
            struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
            uint32_t ip = ntohl(addr->sin_addr.s_addr);
            ip_bytes[0] = (ip >> 24) & 0xFF;
            ip_bytes[1] = (ip >> 16) & 0xFF;
            ip_bytes[2] = (ip >> 8) & 0xFF;
            ip_bytes[3] = ip & 0xFF;
            found = 1;
        }
    }
    
    close(sockfd);
    
    if (!found) {
        // 如果没有找到有效的IP，返回默认值 0.0.0.0
        memset(ip_bytes, 0, 4);
        return -1;
    }
    
    return 0;
}

// 发送IP地址数据
static int send_ip_address_data(int fd) {
    uint8_t ip_bytes[4];
    if (get_local_ip_address(ip_bytes) != 0) {
        // 如果获取失败，发送 0.0.0.0
        memset(ip_bytes, 0, 4);
    }
    
    uint8_t frame[9] = {
        0xAA, 0x55, 0x04,
        ip_bytes[0],  // 192
        ip_bytes[1],  // 168
        ip_bytes[2],  // 1
        ip_bytes[3],  // 1
        0x55, 0xAA
    };

    ssize_t sent = write(fd, frame, sizeof(frame));
    if (sent != sizeof(frame)) {
        return -1;
    }

    printf("[Voice] Sent IP address: %d.%d.%d.%d\n", 
           ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    return 0;
}
static uint8_t weather_text_to_code(const char* text) {
    if (strcmp(text, "晴") == 0) return 0x01;
    if (strcmp(text, "多云") == 0) return 0x02;
    if (strcmp(text, "阴") == 0) return 0x03;
    if (strcmp(text, "雾") == 0) return 0x04;
    if (strcmp(text, "小雨") == 0) return 0x05;
    if (strcmp(text, "中雨") == 0) return 0x06;
    if (strcmp(text, "大雨") == 0) return 0x07;
    if (strcmp(text, "暴雨") == 0) return 0x08;
    if (strcmp(text, "阵雨") == 0) return 0x09;
    if (strcmp(text, "雷阵雨") == 0) return 0x0a;
    if (strcmp(text, "小雪") == 0) return 0x0b;
    if (strcmp(text, "中雪") == 0) return 0x0c;
    if (strcmp(text, "大雪") == 0) return 0x0d;
    if (strcmp(text, "雨夹雪") == 0) return 0x0e;
    if (strcmp(text, "阵雪") == 0) return 0x0f;
    if (strcmp(text, "霾") == 0) return 0x10;
    return 0x00; // 未知
}

// 读取并编码天气数据
static int encode_weather_data(uint8_t* outbuf, size_t outlen) {
    if (outlen < 14) return -1;
    FILE* f = fopen("/development/tmp/weather.json", "r");
    if (!f) return -2;
    int fd = fileno(f);
    if (flock(fd, LOCK_SH) != 0) { fclose(f); return -3; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 4096) { flock(fd, LOCK_UN); fclose(f); return -4; }
    char* buf = malloc(sz+1);
    if (!buf) { flock(fd, LOCK_UN); fclose(f); return -5; }
    fread(buf, 1, sz, f); buf[sz] = 0;
    flock(fd, LOCK_UN); fclose(f);
    struct json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root) return -6;
    struct json_object* now = NULL;
    if (!json_object_object_get_ex(root, "now", &now)) { json_object_put(root); return -7; }
    struct json_object *jtext, *jtemp, *jhum;
    if (!json_object_object_get_ex(now, "text", &jtext) ||
        !json_object_object_get_ex(now, "temp", &jtemp) ||
        !json_object_object_get_ex(now, "humidity", &jhum)) {
        json_object_put(root); return -8;
    }
    const char* text = json_object_get_string(jtext);
    int temp = atoi(json_object_get_string(jtemp));
    int hum = atoi(json_object_get_string(jhum));
    uint8_t weather_code = weather_text_to_code(text);
    // 编码
    memset(outbuf, 0, 14);
    outbuf[0] = 0xAA; outbuf[1] = 0x55; outbuf[2] = 0x01;
    outbuf[3] = weather_code;
    // 温度（4字节，低字节在前，负数补码）
    outbuf[4] = (uint8_t)(temp & 0xFF);
    outbuf[5] = (uint8_t)((temp >> 8) & 0xFF);
    outbuf[6] = (uint8_t)((temp >> 16) & 0xFF);
    outbuf[7] = (uint8_t)((temp >> 24) & 0xFF);
    // 湿度（4字节，低字节在前）
    outbuf[8] = (uint8_t)(hum & 0xFF);
    outbuf[9] = (uint8_t)((hum >> 8) & 0xFF);
    outbuf[10] = (uint8_t)((hum >> 16) & 0xFF);
    outbuf[11] = (uint8_t)((hum >> 24) & 0xFF);
    outbuf[12] = 0x55; outbuf[13] = 0xAA;
    json_object_put(root);
    return 0;
}
static int open_voice_port(void) {
    int fd = -1;
    int retry = 0;
    const int MAX_RETRY = 15;      // 最多重试 15 次（约 30 秒）
    const int RETRY_DELAY_SEC = 2; // 每次间隔 2 秒

    while (retry < MAX_RETRY) {
        fd = open(VOICE_SERIAL_DEVICE, O_RDWR | O_NOCTTY);
        if (fd >= 0) {
            // 成功打开，配置串口
            struct termios tty;
            if (tcgetattr(fd, &tty) != 0) {
                perror("[Voice] tcgetattr failed");
                close(fd);
                return -1;
            }
            cfmakeraw(&tty);
            cfsetspeed(&tty, VOICE_BAUDRATE);
            if (tcsetattr(fd, TCSANOW, &tty) != 0) {
                perror("[Voice] tcsetattr failed");
                close(fd);
                return -1;
            }
            tcflush(fd, TCIOFLUSH);
            printf("[Voice] Serial port opened: %s\n", VOICE_SERIAL_DEVICE);
            return fd;
        }

        // 打开失败
        fprintf(stderr, "[Voice] Failed to open %s (attempt %d/%d): %s\n",
                VOICE_SERIAL_DEVICE, retry + 1, MAX_RETRY, strerror(errno));

        retry++;
        if (retry < MAX_RETRY) {
            sleep(RETRY_DELAY_SEC);
        }
    }

    fprintf(stderr, "[Voice] Giving up after %d attempts. Voice control disabled.\n", MAX_RETRY);
    return -1;
}
// --------------------------------------------------
void* voice_thread(void* arg) {
    int fd = open_voice_port();  // Use the new function
    if (fd < 0) {
        fprintf(stderr, "[Voice] Failed to open %s: %s\n", VOICE_SERIAL_DEVICE, strerror(errno));
        return NULL;
    }
    printf("[Voice] Listening on %s @ %d bps\n", VOICE_SERIAL_DEVICE,
           (VOICE_BAUDRATE == B9600) ? 9600 :
           (VOICE_BAUDRATE == B115200) ? 115200 : 0);

    uint8_t byte;
    while (1) {
        ssize_t n = read(fd, &byte, 1);
        if (n <= 0) {
            usleep(5000);
            continue;
        }

        if (byte == 0x11) {
            uint8_t cmd_code;
            if (read(fd, &cmd_code, 1) == 1) {
                if (cmd_code == 0xFF) {
                    // 处理天气查询命令
                    uint8_t buf[14];
                    if (encode_weather_data(buf, sizeof(buf)) == 0) {
                        write(fd, buf, 14);
                        printf("[Voice] 11 FF: send weather to serial\n");
                    } else {
                        printf("[Voice] 11 FF: weather encode/send failed\n");
                    }
                }
                else if (cmd_code == 0x03){
                    if (send_temperature_data(fd) == 0) {
                        printf("[Voice] 11 03: send temperature to serial\n");
                    } else {
                        printf("[Voice] 11 03: temperature read/send failed\n");
                    }
                }
                else if (cmd_code == 0x04) {
                    // 处理湿度查询命令
                    if (send_humidity_data(fd) == 0) {
                        printf("[Voice] 11 04: send humidity to serial\n");
                    } else {
                        printf("[Voice] 11 04: humidity read/send failed\n");
                    }
                }
                else if (cmd_code == 0x11) {
                    // 处理IP地址查询命令
                    if (send_ip_address_data(fd) == 0) {
                        printf("[Voice] 11 11: send IP address to serial\n");
                    } else {
                        printf("[Voice] 11 11: IP address read/send failed\n");
                    }
                }
                else {
                    const char* zigbee_cmd = NULL;
                    switch (cmd_code) {
                        case 0x01: zigbee_cmd = ZIGBEE_CMD_LIGHT_ON; break;
                        case 0x02: zigbee_cmd = ZIGBEE_CMD_LIGHT_OFF; break;
                        case 0x05: zigbee_cmd = ZIGBEE_CMD_FAN_ON; break;
                        case 0x06: zigbee_cmd = ZIGBEE_CMD_FAN_OFF; break;
                        case 0x07: zigbee_cmd = ZIGBEE_CMD_AIRCON_ON; break;
                        case 0x08: zigbee_cmd = ZIGBEE_CMD_AIRCON_OFF; break;
                        case 0x09: zigbee_cmd = ZIGBEE_CMD_DOOR_OPEN; break;
                        case 0x10: zigbee_cmd = ZIGBEE_CMD_DOOR_CLOSE; break;
                        case 0x12: zigbee_cmd = ZIGBEE_CMD_WASHING_ON; break;
                        case 0x13: zigbee_cmd = ZIGBEE_CMD_WASHING_OFF; break;
                        default:
                            continue; // 忽略查询命令
                    }
                    if (zigbee_cmd) {
                        printf("[Voice] Rx: 11 %02X → %s\n", cmd_code, zigbee_cmd);
                        send_zigbee_command(zigbee_cmd);
                    }
                }
            }
        }
    }

    close(fd);
    return NULL;
}