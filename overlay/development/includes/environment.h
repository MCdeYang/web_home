#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
struct MemoryStruct {
    char *memory;
    size_t size;
};
void *environment_thread(void *arg);
#endif