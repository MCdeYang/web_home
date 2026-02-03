/**********************************************************************
 * @file main.c
 * @brief 主函数
 *
 *
 * @author 杨翊
 * @date 2026-01-23
 * @version 1.0
 *
 * @note
 *
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "environment.h"
#include "check_task.h"
#include "temperature.h"
#include "zigbee_mq.h"
#include "voice.h"

int main() {
    pthread_t env_thread, task_thread, temp_thread, zig_thread, vo_thread;
    //初始化 Zigbee 消息队列
    if (init_zigbee_mq() != 0) {
        fprintf(stderr, "Failed to init MQ\n");
        return 1;
    }
    //创建各个线程
    if (pthread_create(&env_thread, NULL, environment_thread, NULL) != 0) {
        perror("Failed to create environment thread");
        return EXIT_FAILURE;
    }
    if (pthread_create(&task_thread, NULL, check_task_thread, NULL) != 0) {
        perror("Failed to create check_task thread");
        return EXIT_FAILURE;
    }
    if (pthread_create(&temp_thread, NULL, temperature_thread, NULL) != 0) {
        perror("Failed to create temperature thread");
        return EXIT_FAILURE;
    }
    if (pthread_create(&zig_thread, NULL, zigbee_thread, NULL) != 0) {
        perror("Failed to create zigbee thread");
        return EXIT_FAILURE;
    }
    if (pthread_create(&vo_thread, NULL, voice_thread, NULL) != 0) {
        perror("Failed to create voice thread");
        return EXIT_FAILURE;
    }

    pthread_join(env_thread, NULL);
    pthread_join(task_thread, NULL);
    pthread_join(temp_thread, NULL);
    pthread_join(zig_thread, NULL);
    pthread_join(vo_thread, NULL);

    return EXIT_SUCCESS;
}