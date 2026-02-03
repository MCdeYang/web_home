#ifndef WIFI_H
#define WIFI_H

#include <stddef.h>
#include "define.h"

int run_command_capture(const char*cmd,char*output,size_t out_size);
int is_safe_string(const char*str,int allow_space);

#endif
