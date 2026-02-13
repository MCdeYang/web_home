#ifndef COMMON_H
#define COMMON_H

#include <json-c/json.h>

void send_json_headers(void);
void send_json_object_response(json_object*obj);

#endif
