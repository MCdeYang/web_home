// system.h
#ifndef SYSTEM_H
#define SYSTEM_H

typedef struct {
    double load_percent;
    long uptime_minutes;
    long memory_total_mb;
    long memory_used_mb;
    double memory_percent;
    char ip[64];
    double cpu_temp_c;      // -1.0 表示不可用
    unsigned long disk_total_gb;
    unsigned long disk_used_gb;
    double disk_percent;
} system_info_t;

// 填充 system_info_t 结构体
int collect_system_info(system_info_t *info);

#endif // SYSTEM_H