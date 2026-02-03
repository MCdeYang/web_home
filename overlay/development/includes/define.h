#ifndef DEFINE_H
#define DEFINE_H
// check_task.h
#define DATA_FILE    "/development/tmp/members.json"
#define STATE_FILE   "/development/tmp/check_tasks.state"
#define CHECK_INTERVAL 30
// environment.h
#define WEATHER_CACHE_FILE "/development/tmp/weather.json"
#define CITY_ID "101060501"          // 吉林省通化市
#define API_KEY "8464c2a756f64b2d81da04e28529fc53"
#define API_HOST "nb3yfrku57.re.qweatherapi.com"
#define WEATHER_INTERVAL 600   // 10分钟
// temperature.h
#define SERIAL_PORT     "/dev/ttyS4"
#define BAUD_RATE       B9600
#define CMD             "Read\r\n"
#define CMD_LEN         (sizeof(CMD) - 1)
#define BUFFER_SIZE     256
#define OUTPUT_FILE     "/development/tmp/temperature.json"
#define INTERVAL_SEC    10
// voice.h
#define VOICE_SERIAL_DEVICE    "/dev/voice_module"
#define VOICE_BAUDRATE         B9600
// zigbee_mq.h
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
#define ZIGBEE_CMD_WIFI_ON      "WIFI1"
#define ZIGBEE_CMD_WIFI_OFF     "WIFI0"
#define MQ_NAME        "/zigbee_cmd"
#define MQ_MAX_MSG     10
#define MAX_CMD_LEN    32
#define SERIAL_DEVICE  "/dev/zigbee_module"
#define DEVICE_STATE_FILE "/development/tmp/device_state.txt"
#define MAX_RETRY 10
#define RETRY_DELAY_SEC 2
#endif
