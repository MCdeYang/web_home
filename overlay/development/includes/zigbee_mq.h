// zigbee_mq.h
#ifndef ZIGBEE_MQ_H
#define ZIGBEE_MQ_H

#include <mqueue.h>
#define SERIAL_DEVICE "/dev/ttyUSB4"
#define MQ_NAME      "/zigbee_cmd"
#define MAX_CMD_LEN  16
#define MQ_MAX_MSG   10

#define ZIGBEE_CMD_LIGHT_ON      "L1"
#define ZIGBEE_CMD_LIGHT_OFF     "L0"

#define ZIGBEE_CMD_AIRCON_ON     "A1"
#define ZIGBEE_CMD_AIRCON_OFF    "A0"

#define ZIGBEE_CMD_WASHING_ON    "W1"
#define ZIGBEE_CMD_WASHING_OFF   "W0"

#define ZIGBEE_CMD_FAN_ON        "F1"
#define ZIGBEE_CMD_FAN_OFF       "F0"

#define ZIGBEE_CMD_DOOR_OPEN     "D1"
#define ZIGBEE_CMD_DOOR_CLOSE    "D0"

// environment 内部使用
int init_zigbee_mq(void);
int send_zigbee_command(const char* cmd);      // voice 线程调用
void* zigbee_thread(void* arg);

// ✅ CGI 专用（关键！）
int cgi_send_zigbee_cmd(const char* cmd);      // CGI 进程调用

#endif