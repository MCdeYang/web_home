// system.c
#define _GNU_SOURCE 
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <math.h> // for round()

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>        // 提供 struct ifaddrs, getifaddrs, freeifaddrs
#include <errno.h>

// 读取 /proc 类文件第一行
static int read_proc_file(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(buf, size, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    return 1;
}

// ✅ 获取真实局域网 IP（使用 getifaddrs，不依赖 ifconfig）
static void get_ip_address(char *ip, size_t len) {
    if (!ip || len < 16) {
        if (ip) ip[0] = '\0';
        return;
    }

    struct ifaddrs *ifs = NULL;
    if (getifaddrs(&ifs) != 0) {
        strncpy(ip, "127.0.0.1", len - 1);
        return;
    }

    const char *preferred_ifs[] = {"eth0", "wlan0", "br-lan", "enp0s20u1"};
    int found = 0;

    // 优先匹配常用接口
    for (int i = 0; i < 4 && !found; i++) {
        for (struct ifaddrs *ifa = ifs; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue; // 跳过 127.0.0.1
            if (!(ifa->ifa_flags & IFF_UP)) continue;    // 接口必须启用

            if (strcmp(ifa->ifa_name, preferred_ifs[i]) == 0) {
                struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
                if (inet_ntop(AF_INET, &sin->sin_addr, ip, len)) {
                    found = 1;
                    break;
                }
            }
        }
    }

    // 若未找到，取第一个非 loopback 的 IPv4
    if (!found) {
        for (struct ifaddrs *ifa = ifs; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &sin->sin_addr, ip, len)) {
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        strncpy(ip, "127.0.0.1", len - 1);
    }

    freeifaddrs(ifs);
}

// 获取 CPU 温度（支持 RK356x / 树莓派等）
static double get_cpu_temp_c(void) {
    const char *paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
        "/sys/class/hwmon/hwmon1/temp1_input"
    };
    char buf[32];
    for (int i = 0; i < 3; i++) {
        if (read_proc_file(paths[i], buf, sizeof(buf))) {
            long temp = atol(buf);
            if (temp > 1000) temp /= 1000; // 转为 °C
            return (double)temp;
        }
    }
    return -1.0; // 无效值
}

// 主采集函数
int collect_system_info(system_info_t *info) {
    if (!info) return 0;

    // 初始化
    memset(info, 0, sizeof(*info));
    info->cpu_temp_c = -1.0;

    // 1. CPU 负载（保留1位小数）
    double load_avg[3];
    if (getloadavg(load_avg, 3) != -1) {
        int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        if (ncpu <= 0) ncpu = 1;
        double percent = (load_avg[0] / ncpu) * 100.0;
        if (percent > 100.0) percent = 100.0;
        info->load_percent = round(percent * 10) / 10.0;
    }

    // 2. 运行时间
    char uptime_str[64];
    if (read_proc_file("/proc/uptime", uptime_str, sizeof(uptime_str))) {
        double uptime_sec = atof(uptime_str);
        info->uptime_minutes = (long)(uptime_sec / 60);
    }

    // 3. 内存
    long mem_total_kb = 0, mem_available_kb = 0;
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[128];
        while (fgets(line, sizeof(line), meminfo)) {
            sscanf(line, "MemTotal: %ld kB", &mem_total_kb);
            sscanf(line, "MemAvailable: %ld kB", &mem_available_kb);
        }
        fclose(meminfo);

        long mem_used_kb = mem_total_kb - mem_available_kb;
        // 如果 MemAvailable 不存在（老内核），回退到 MemFree
        if (mem_available_kb == 0) {
            // 重新读取 MemFree
            FILE *f2 = fopen("/proc/meminfo", "r");
            if (f2) {
                while (fgets(line, sizeof(line), f2)) {
                    long mem_free_kb = 0;
                    if (sscanf(line, "MemFree: %ld kB", &mem_free_kb) == 1) {
                        mem_used_kb = mem_total_kb - mem_free_kb;
                        break;
                    }
                }
                fclose(f2);
            }
        }

        info->memory_total_mb = mem_total_kb / 1024;
        info->memory_used_mb = mem_used_kb / 1024;

        if (mem_total_kb > 0) {
            double percent = (double)mem_used_kb / mem_total_kb * 100.0;
            info->memory_percent = round(percent * 10) / 10.0;
        }
    }

    // 4. IP 地址（✅ 修复版）
    get_ip_address(info->ip, sizeof(info->ip));

    // 5. CPU 温度
    info->cpu_temp_c = get_cpu_temp_c();

    // 6. 磁盘（根分区）
    struct statvfs fs;
    if (statvfs("/", &fs) == 0) {
        unsigned long long total_bytes = (unsigned long long)fs.f_frsize * fs.f_blocks;
        unsigned long long free_bytes = (unsigned long long)fs.f_frsize * fs.f_bfree;
        unsigned long long used_bytes = total_bytes - free_bytes;

        info->disk_total_gb = (unsigned long)(total_bytes / (1024ULL * 1024 * 1024));
        info->disk_used_gb = (unsigned long)(used_bytes / (1024ULL * 1024 * 1024));

        if (info->disk_total_gb > 0) {
            double percent = (double)info->disk_used_gb / info->disk_total_gb * 100.0;
            info->disk_percent = round(percent * 10) / 10.0;
        }
    }

    return 1;
}