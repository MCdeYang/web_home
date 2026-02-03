#ifndef FUNCTION_H
#define FUNCTION_H

void check_auth_get(const char*path,const char*body);
void weather_get(const char*path,const char*body);
void temperature_get(const char*path,const char*body);
void picture_get(const char*path,const char*body);
void notice_get(const char*path,const char*body);
void system_get(const char*path,const char*body);
void disk_download_get(const char*path,const char*body);
void disk_list_get(const char*path,const char*body);
void photos_list_get(const char*path,const char*body);
void photos_photo_get(const char*path,const char*body);
void family_members_get(const char*path,const char*body);
void photo_note_get(const char*path,const char*body);
void family_my_tasks_get(const char*path,const char*body);
void control_light_get(const char*path,const char*body);
void control_aircon_get(const char*path,const char*body);
void control_washing_machine_get(const char*path,const char*body);
void control_fan_get(const char*path,const char*body);
void control_door_get(const char*path,const char*body);
void settings_change_password_get(const char*path,const char*body);
void settings_public_get(const char*path,const char*body);
void settings_wifi_get(const char*path,const char*body);

void control_light_put(const char*path,const char*body);
void control_aircon_put(const char*path,const char*body);
void control_washing_machine_put(const char*path,const char*body);
void control_fan_put(const char*path,const char*body);
void control_door_put(const char*path,const char*body);
void settings_public_put(const char*path,const char*body);
void settings_wifi_put(const char*path,const char*body);

void login_post(const char*path,const char*body);
void disk_upload_post(const char*path,const char*body);
void disk_mkdir_post(const char*path,const char*body);
void disk_rename_post(const char*path,const char*body);
void photos_upload(const char*path,const char*body);
void photo_note_post(const char*path,const char*body);
void family_members_post(const char*path,const char*body);
void family_task_post(const char*path,const char*body);
void settings_change_password_post(const char*path,const char*body);

void disk_delete_handler(const char*path,const char*body);
void photos_delete(const char*path,const char*body);

#endif
