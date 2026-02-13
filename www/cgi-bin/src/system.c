/**********************************************************************
 * @file system.c
 * @brief 系统信息采集模块实现
 *
 * 本文件实现负载内存磁盘网络等系统信息采集
 * 供 CGI 处理函数返回系统状态数据
 *
 * @author 杨翊
 * @date 2026-02-02
 * @version 1.0
 *
 * @note
 * - 通过读取系统接口与 proc 信息获取运行状态
 **********************************************************************/
#define _GNU_SOURCE 
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <math.h> // for round

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>        // 提供 struct ifaddrs, getifaddrs, freeifaddrs
#include <errno.h>

// 读取 proc 文件第一行
static int read_proc_file(const char*path,char*buf,size_t size){
    FILE*f;
    char*nl;

    f=fopen(path,"r");
    if(!f){
        return 0;
    }
    if(!fgets(buf,(int)size,f)){
        fclose(f);
        return 0;
    }
    fclose(f);
    nl=strchr(buf,'\n');
    if(nl){
        *nl='\0';
    }
    return 1;
}

// 获取设备 IPv4 地址
static void get_ip_address(char*ip,size_t len){
    struct ifaddrs*ifs;
    const char*preferred_ifs[4];
    int found;
    int i;
    struct ifaddrs*ifa;
    struct sockaddr_in*sin;

    if(!ip||len<16){
        if(ip){
            ip[0]='\0';
        }
        return;
    }

    ifs=NULL;
    if(getifaddrs(&ifs)!=0){
        strncpy(ip,"127.0.0.1",len-1);
        ip[len-1]='\0';
        return;
    }

    preferred_ifs[0]="eth0";
    preferred_ifs[1]="wlan0";
    preferred_ifs[2]="br-lan";
    preferred_ifs[3]="enp0s20u1";
    found=0;

    for(i=0;i<4&&!found;i++){
        for(ifa=ifs;ifa!=NULL;ifa=ifa->ifa_next){
            if(ifa->ifa_addr==NULL){
                continue;
            }
            if(ifa->ifa_addr->sa_family!=AF_INET){
                continue;
            }
            if(ifa->ifa_flags&IFF_LOOPBACK){
                continue;
            }
            if(!(ifa->ifa_flags&IFF_UP)){
                continue;
            }
            if(strcmp(ifa->ifa_name,preferred_ifs[i])==0){
                sin=(struct sockaddr_in*)ifa->ifa_addr;
                if(inet_ntop(AF_INET,&sin->sin_addr,ip,len)){
                    found=1;
                    break;
                }
            }
        }
    }

    if(!found){
        for(ifa=ifs;ifa!=NULL;ifa=ifa->ifa_next){
            if(ifa->ifa_addr==NULL){
                continue;
            }
            if(ifa->ifa_addr->sa_family!=AF_INET){
                continue;
            }
            if(ifa->ifa_flags&IFF_LOOPBACK){
                continue;
            }
            if(!(ifa->ifa_flags&IFF_UP)){
                continue;
            }
            sin=(struct sockaddr_in*)ifa->ifa_addr;
            if(inet_ntop(AF_INET,&sin->sin_addr,ip,len)){
                found=1;
                break;
            }
        }
    }

    if(!found){
        strncpy(ip,"127.0.0.1",len-1);
        ip[len-1]='\0';
    }
    freeifaddrs(ifs);
}

// 获取 CPU 温度
static double get_cpu_temp_c(void){
    const char*paths[3];
    char buf[32];
    int i;
    long temp;

    paths[0]="/sys/class/thermal/thermal_zone0/temp";
    paths[1]="/sys/class/hwmon/hwmon0/temp1_input";
    paths[2]="/sys/class/hwmon/hwmon1/temp1_input";
    for(i=0;i<3;i++){
        if(read_proc_file(paths[i],buf,sizeof(buf))){
            temp=atol(buf);
            if(temp>1000){
                temp/=1000;
            }
            return (double)temp;
        }
    }
    return -1.0;
}

// 采集系统信息
int collect_system_info(system_info_t*info){
    double load_avg[3];
    int ncpu;
    double percent;
    char uptime_str[64];
    double uptime_sec;
    long mem_total_kb;
    long mem_available_kb;
    FILE*meminfo;
    char line[128];
    long mem_used_kb;
    FILE*f2;
    long mem_free_kb;
    struct statvfs fs;
    unsigned long long total_bytes;
    unsigned long long free_bytes;
    unsigned long long used_bytes;

    if(!info){
        return 0;
    }

    memset(info,0,sizeof(*info));
    info->cpu_temp_c=-1.0;

    if(getloadavg(load_avg,3)!=-1){
        ncpu=(int)sysconf(_SC_NPROCESSORS_ONLN);
        if(ncpu<=0){
            ncpu=1;
        }
        percent=(load_avg[0]/ncpu)*100.0;
        if(percent>100.0){
            percent=100.0;
        }
        info->load_percent=round(percent*10)/10.0;
    }

    if(read_proc_file("/proc/uptime",uptime_str,sizeof(uptime_str))){
        uptime_sec=atof(uptime_str);
        info->uptime_minutes=(long)(uptime_sec/60);
    }

    mem_total_kb=0;
    mem_available_kb=0;
    meminfo=fopen("/proc/meminfo","r");
    if(meminfo){
        while(fgets(line,sizeof(line),meminfo)){
            sscanf(line,"MemTotal: %ld kB",&mem_total_kb);
            sscanf(line,"MemAvailable: %ld kB",&mem_available_kb);
        }
        fclose(meminfo);
        mem_used_kb=mem_total_kb-mem_available_kb;
        if(mem_available_kb==0){
            f2=fopen("/proc/meminfo","r");
            if(f2){
                while(fgets(line,sizeof(line),f2)){
                    mem_free_kb=0;
                    if(sscanf(line,"MemFree: %ld kB",&mem_free_kb)==1){
                        mem_used_kb=mem_total_kb-mem_free_kb;
                        break;
                    }
                }
                fclose(f2);
            }
        }
        info->memory_total_mb=mem_total_kb/1024;
        info->memory_used_mb=mem_used_kb/1024;
        if(mem_total_kb>0){
            percent=((double)mem_used_kb/mem_total_kb)*100.0;
            info->memory_percent=round(percent*10)/10.0;
        }
    }

    get_ip_address(info->ip,sizeof(info->ip));
    info->cpu_temp_c=get_cpu_temp_c();
    if(statvfs("/",&fs)==0){
        total_bytes=(unsigned long long)fs.f_frsize*fs.f_blocks;
        free_bytes=(unsigned long long)fs.f_frsize*fs.f_bfree;
        used_bytes=total_bytes-free_bytes;
        info->disk_total_gb=(unsigned long)(total_bytes/(1024ULL*1024*1024));
        info->disk_used_gb=(unsigned long)(used_bytes/(1024ULL*1024*1024));
        if(info->disk_total_gb>0){
            percent=((double)info->disk_used_gb/info->disk_total_gb)*100.0;
            info->disk_percent=round(percent*10)/10.0;
        }
    }
    return 1;
}
