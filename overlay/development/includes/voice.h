#ifndef VOICE_H
#define VOICE_H

#include <stdint.h>

// ========== 配置宏（集中管理）==========
#define VOICE_SERIAL_DEVICE    "/dev/voice_module"
#define VOICE_BAUDRATE         B9600

// 可选：调试开关
// #define VOICE_DEBUG

// ========== 函数声明 ==========
void* voice_thread(void* arg);

#endif // VOICE_H