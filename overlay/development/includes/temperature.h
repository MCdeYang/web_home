#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#define SERIAL_PORT     "/dev/ttyS4"
#define BAUD_RATE       B9600
#define CMD             "Read\r\n"
#define CMD_LEN         (sizeof(CMD) - 1)
#define BUFFER_SIZE     256
#define OUTPUT_FILE     "/development/tmp/temperature.json"
#define INTERVAL_SEC    10

void* temperature_thread(void *arg);

#endif // TEMPERATURE_H