#ifndef FAMILY_H
#define FAMILY_H

#include <time.h>
#include <json-c/json.h>
#include "define.h"

char*get_username_from_cookie(void);
json_object*load_family_data(void);
int save_family_data(json_object*root);
time_t parse_due_date(const char*due_str);

#endif
