// voice.c
#include "voice.h"
#include "zigbee_mq.h"  // 包含 ZIGBEE_CMD_XXX 和 send_zigbee_command
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <stdint.h>
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
                    // ✅ 关键：调用现有函数，自动更新状态 + 发 MQ
                    send_zigbee_command(zigbee_cmd);
                }
            }
        }
    }

    close(fd);
    return NULL;
}