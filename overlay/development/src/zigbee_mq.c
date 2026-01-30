#include "zigbee_mq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <errno.h>
#include <mqueue.h>

static mqd_t g_mq_fd = (mqd_t)-1;
static int g_serial_fd = -1;


// 写锁（用于 environment 内部日志/调试）
pthread_mutex_t g_mq_write_mutex = PTHREAD_MUTEX_INITIALIZER;

// 合法命令列表（共享）
static const char* valid_commands[] = {
    ZIGBEE_CMD_LIGHT_ON, ZIGBEE_CMD_LIGHT_OFF,
    ZIGBEE_CMD_AIRCON_ON, ZIGBEE_CMD_AIRCON_OFF,
    ZIGBEE_CMD_WASHING_ON, ZIGBEE_CMD_WASHING_OFF,
    ZIGBEE_CMD_FAN_ON, ZIGBEE_CMD_FAN_OFF,
    ZIGBEE_CMD_DOOR_OPEN, ZIGBEE_CMD_DOOR_CLOSE
};
#define VALID_CMD_COUNT (sizeof(valid_commands) / sizeof(valid_commands[0]))

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
    ;
    return 0;
}

// 发送命令（给 environment 内部的 voice 线程使用）
// 加锁仅用于日志/调试一致性，非必需
int send_zigbee_command(const char* cmd) {
    if (!cmd || !is_valid_command(cmd)) {
        return -1;
    }

    pthread_mutex_lock(&g_mq_write_mutex);

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

// ✅ 新增：CGI 专用发送函数（无全局依赖）
int cgi_send_zigbee_cmd(const char* cmd) {
    if (!cmd || !is_valid_command(cmd)) {
        return -1;
    }

    size_t len = strlen(cmd);
    if (len >= MAX_CMD_LEN) {
        return -1;
    }

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

        // ⚠️ 注意：你的 Zigbee 模块是否需要换行？
        // 大多数串口设备需要 \r\n 或 \n 结尾
        // 如果当前不工作，请尝试：
        //   write(g_serial_fd, buffer, bytes_read);
        //   write(g_serial_fd, "\r\n", 2);
        //
        // 目前按你原代码：只发原始命令
        write(g_serial_fd, buffer, bytes_read);
        printf("[Zigbee] Sent: %.*s\n", (int)bytes_read, buffer);

        usleep(10000); // 10ms
    }

    close(g_serial_fd);
    return NULL;
}