#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "environment.h"
#include "check_task.h"

int main() {
    pthread_t env_thread, task_thread;

    if (pthread_create(&env_thread, NULL, environment_thread, NULL) != 0) {
        perror("Failed to create environment thread");
        return EXIT_FAILURE;
    }

    if (pthread_create(&task_thread, NULL, check_task_thread, NULL) != 0) {
        perror("Failed to create check_task thread");
        return EXIT_FAILURE;
    }

    pthread_join(env_thread, NULL);
    pthread_join(task_thread, NULL);

    return EXIT_SUCCESS;
}