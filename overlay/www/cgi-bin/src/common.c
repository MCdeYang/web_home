#include <stdio.h>

void send_json_headers(void) {
    printf("Content-Type: application/json; charset=utf-8\r\n");
    printf("Cache-Control: no-cache\r\n");
    printf("\r\n"); // 空行结束头部
}
