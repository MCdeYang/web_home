#ifndef FAMILY_H
#define FAMILY_H

#define FAMILY_DATA_PATH "/development/tmp/members.json"

char* get_username_from_cookie();
json_object* load_family_data();
int save_family_data(json_object *root);
time_t parse_due_date(const char *due_str);

#endif