#ifndef ZIGBEE_MQ_H
#define ZIGBEE_MQ_H

#include <stddef.h>

// 命令定义
#define ZIGBEE_CMD_LIGHT_ON     "L1"
#define ZIGBEE_CMD_LIGHT_OFF    "L0"
#define ZIGBEE_CMD_AIRCON_ON    "A1"
#define ZIGBEE_CMD_AIRCON_OFF   "A0"
#define ZIGBEE_CMD_WASHING_ON   "W1"
#define ZIGBEE_CMD_WASHING_OFF  "W0"
#define ZIGBEE_CMD_FAN_ON       "F1"
#define ZIGBEE_CMD_FAN_OFF      "F0"
#define ZIGBEE_CMD_DOOR_OPEN    "D1"
#define ZIGBEE_CMD_DOOR_CLOSE   "D0"

// 配置
#define MQ_NAME        "/zigbee_cmd"
#define MQ_MAX_MSG     10
#define MAX_CMD_LEN    32
#define SERIAL_DEVICE  "/dev/ttyUSB4"  // 👈 请根据实际设备修改！

// 函数声明
int init_zigbee_mq(void);
int send_zigbee_command(const char* cmd);
int cgi_send_zigbee_cmd(const char* cmd);
void* zigbee_thread(void* arg);

// 状态查询（CGI 使用）
int get_light_state(void);
int get_fan_state(void);
int get_aircon_state(void);
int get_washing_state(void);
int get_door_state(void);

#endif