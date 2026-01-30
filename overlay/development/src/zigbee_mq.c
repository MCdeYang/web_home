#include <stdio.h>
#include <sys/file.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <errno.h>
#include <mqueue.h>
#include <sys/types.h>
#include <dirent.h>
#include "zigbee_mq.h"

static mqd_t g_mq_fd = (mqd_t)-1;
static int g_serial_fd = -1;

// 写锁（用于 environment 内部日志/调试）
pthread_mutex_t g_mq_write_mutex = PTHREAD_MUTEX_INITIALIZER;

// 状态文件路径
#define STATE_FILE "/development/tmp/device_state.txt"

// 合法命令列表（共享）
static const char* valid_commands[] = {
    ZIGBEE_CMD_LIGHT_ON, ZIGBEE_CMD_LIGHT_OFF,
    ZIGBEE_CMD_AIRCON_ON, ZIGBEE_CMD_AIRCON_OFF,
    ZIGBEE_CMD_WASHING_ON, ZIGBEE_CMD_WASHING_OFF,
    ZIGBEE_CMD_FAN_ON, ZIGBEE_CMD_FAN_OFF,
    ZIGBEE_CMD_DOOR_OPEN, ZIGBEE_CMD_DOOR_CLOSE
};
#define VALID_CMD_COUNT (sizeof(valid_commands) / sizeof(valid_commands[0]))

// 创建目录（递归创建 /development/tmp）
static int create_state_dir(void) {
    // 确保 /development 存在
    if (mkdir("/development", 0755) == -1 && errno != EEXIST) {
        perror("mkdir /development");
        return -1;
    }
    // 确保 /development/tmp 存在
    if (mkdir("/development/tmp", 0755) == -1 && errno != EEXIST) {
        perror("mkdir /development/tmp");
        return -1;
    }
    return 0;
}

// 从文件加载当前状态到内存（可选，用于 environment 内部一致性）
static void load_state_from_file(int* light, int* fan, int* aircon, int* washing, int* door) {
    FILE* f = fopen(STATE_FILE, "r");
    if (!f) {
        // 文件不存在，使用默认值（全关）
        *light = *fan = *aircon = *washing = *door = 0;
        return;
    }
    if (flock(fileno(f), LOCK_SH) != 0) {
        perror("flock SH");
        fclose(f);
        *light = *fan = *aircon = *washing = *door = 0;
        return;
    }
    int l, f_val, a, w, d;
    if (fscanf(f, "%d %d %d %d %d", &l, &f_val, &a, &w, &d) == 5) {
        *light = l;
        *fan = f_val;
        *aircon = a;
        *washing = w;
        *door = d;
    } else {
        // 格式错误，重置为默认
        *light = *fan = *aircon = *washing = *door = 0;
    }
    flock(fileno(f), LOCK_UN);
    fclose(f);
}

// 将状态写入文件（线程安全）
static void save_state_to_file(int light, int fan, int aircon, int washing, int door) {
    // 确保目录存在
    create_state_dir();

    FILE* f = fopen(STATE_FILE, "w");
    if (!f) {
        perror("fopen state file for writing");
        return;
    }
    if (flock(fileno(f), LOCK_EX) != 0) {
        perror("flock EX");
        fclose(f);
        return;
    }
    fprintf(f, "%d %d %d %d %d\n", light, fan, aircon, washing, door);
    fflush(f); // 确保写入磁盘
    flock(fileno(f), LOCK_UN);
    fclose(f);
}

// 打开 Zigbee 串口
static int open_serial_port(void) {
    int fd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;

    struct termios tty;
    tcgetattr(fd, &tty);
    cfmakeraw(&tty);
    cfsetspeed(&tty, B115200);
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIOFLUSH);
    return fd;
}

// 验证命令（内部函数）
static int is_valid_command(const char* cmd) {
    for (int i = 0; i < VALID_CMD_COUNT; i++) {
        if (strcmp(cmd, valid_commands[i]) == 0)
            return 1;
    }
    return 0;
}

// 初始化消息队列（由 environment 主进程调用）
int init_zigbee_mq(void) {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSG;
    attr.mq_msgsize = MAX_CMD_LEN;
    attr.mq_curmsgs = 0;
    mode_t old_mask = umask(0);
    g_mq_fd = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    umask(old_mask);
    if (g_mq_fd == (mqd_t)-1) {
        perror("mq_open");
        return -1;
    }
    return 0;
}

// ✅ 根据命令更新状态并保存到文件
static void update_and_save_state(const char* cmd) {
    // 先读取当前状态（避免覆盖其他设备状态）
    int light, fan, aircon, washing, door;
    load_state_from_file(&light, &fan, &aircon, &washing, &door);

    if (strcmp(cmd, ZIGBEE_CMD_LIGHT_ON) == 0) {
        light = 1;
    } else if (strcmp(cmd, ZIGBEE_CMD_LIGHT_OFF) == 0) {
        light = 0;
    } else if (strcmp(cmd, ZIGBEE_CMD_FAN_ON) == 0) {
        fan = 1;
    } else if (strcmp(cmd, ZIGBEE_CMD_FAN_OFF) == 0) {
        fan = 0;
    } else if (strcmp(cmd, ZIGBEE_CMD_AIRCON_ON) == 0) {
        aircon = 1;
    } else if (strcmp(cmd, ZIGBEE_CMD_AIRCON_OFF) == 0) {
        aircon = 0;
    } else if (strcmp(cmd, ZIGBEE_CMD_WASHING_ON) == 0) {
        washing = 1;
    } else if (strcmp(cmd, ZIGBEE_CMD_WASHING_OFF) == 0) {
        washing = 0;
    } else if (strcmp(cmd, ZIGBEE_CMD_DOOR_OPEN) == 0) {
        door = 1;  // 解锁
    } else if (strcmp(cmd, ZIGBEE_CMD_DOOR_CLOSE) == 0) {
        door = 0;  // 锁定
    }

    // 保存回文件
    save_state_to_file(light, fan, aircon, washing, door);
}

// 发送命令（给 environment 内部的 voice 线程使用）
int send_zigbee_command(const char* cmd) {
    if (!cmd || !is_valid_command(cmd)) {
        return -1;
    }

    pthread_mutex_lock(&g_mq_write_mutex);

    update_and_save_state(cmd);  // 更新并保存状态

    if (strlen(cmd) >= MAX_CMD_LEN) {
        pthread_mutex_unlock(&g_mq_write_mutex);
        return -1;
    }

    if (mq_send(g_mq_fd, cmd, strlen(cmd), 1) == -1) {
        perror("mq_send");
        pthread_mutex_unlock(&g_mq_write_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_mq_write_mutex);
    return 0;
}

// CGI 专用发送函数（无全局依赖）
int cgi_send_zigbee_cmd(const char* cmd) {
    if (!cmd || !is_valid_command(cmd)) {
        return -1;
    }

    size_t len = strlen(cmd);
    if (len >= MAX_CMD_LEN) {
        return -1;
    }

    // ✅ 更新状态文件（关键！）
    update_and_save_state(cmd);

    // 自己打开 MQ（只写、非阻塞）
    mqd_t mq = mq_open(MQ_NAME, O_WRONLY | O_NONBLOCK);
    if (mq == (mqd_t)-1) {
        return -2; // MQ 未创建或权限问题
    }

    int ret = 0;
    if (mq_send(mq, cmd, len, 1) == -1) {
        ret = -3; // 发送失败（如队列满）
    }

    mq_close(mq);
    return ret;
}

// ✅ 从文件读取状态（供 main.cgi 调用）
int get_light_state(void) {
    int light, fan, aircon, washing, door;
    load_state_from_file(&light, &fan, &aircon, &washing, &door);
    return light;
}

int get_fan_state(void) {
    int light, fan, aircon, washing, door;
    load_state_from_file(&light, &fan, &aircon, &washing, &door);
    return fan;
}

int get_aircon_state(void) {
    int light, fan, aircon, washing, door;
    load_state_from_file(&light, &fan, &aircon, &washing, &door);
    return aircon;
}

int get_washing_state(void) {
    int light, fan, aircon, washing, door;
    load_state_from_file(&light, &fan, &aircon, &washing, &door);
    return washing;
}

int get_door_state(void) {
    int light, fan, aircon, washing, door;
    load_state_from_file(&light, &fan, &aircon, &washing, &door);
    return door;
}

// Zigbee 线程：从 MQ 读命令并发送到串口
void* zigbee_thread(void* arg) {
    char buffer[MAX_CMD_LEN];
    ssize_t bytes_read;

    g_serial_fd = open_serial_port();
    if (g_serial_fd < 0) {
        perror("Failed to open serial");
        return NULL;
    }

    printf("Zigbee thread started, reading from MQ %s\n", MQ_NAME);

    while (1) {
        bytes_read = mq_receive(g_mq_fd, buffer, MAX_CMD_LEN, NULL);
        if (bytes_read <= 0) continue;

        buffer[bytes_read] = '\0';

        if (!is_valid_command(buffer)) {
            continue;
        }

        write(g_serial_fd, buffer, bytes_read);
        printf("[Zigbee] Sent: %.*s\n", (int)bytes_read, buffer);

        usleep(10000); // 10ms
    }

    close(g_serial_fd);
    return NULL;
}