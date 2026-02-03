#ifndef ZIGBEE_MQ_H
#define ZIGBEE_MQ_H

#include "define.h"

int init_zigbee_mq(void);
int send_zigbee_command(const char* cmd);
int cgi_send_zigbee_cmd(const char* cmd);
void* zigbee_thread(void* arg);
// 状态查询 CGI 使用
int get_light_state(void);
int get_fan_state(void);
int get_aircon_state(void);
int get_washing_state(void);
int get_door_state(void);
#endif