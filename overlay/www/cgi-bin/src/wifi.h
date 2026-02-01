#ifndef WIFI_H_
#define WIFI_H_ 

#define WIFI_START_SCRIPT   "/development/wifi/wifi_start.sh"
#define WIFI_STOP_SCRIPT    "/development/wifi/wifi_stop.sh"
int run_command_capture(const char *cmd, char *output, size_t out_size);
int is_safe_string(const char *str, int allow_space);
#endif