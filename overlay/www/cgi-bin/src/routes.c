#include <stdio.h>
#include <string.h>
#include "routes.h"
#include "function.h"

struct route routes[] = {
    {
        .path = "/check-auth",
        .get = check_auth_get
    },
    {
        .path="/login",
        .post= login_post
    },
    {
        .path = "/weather",
        .get = weather_get
    },
    {
        .path = "/temperature",
        .get = temperature_get
    },
    {
        .path = "/picture",
        .get = picture_get,
        .put = picture_put
        // POST/DELETE not implemented → NULL
    },
    {
        .path = "/disk/download",
        .get = disk_download_get
    },
    {
        .path = "/disk/upload",
        .post = disk_upload_post
    },
    {
        .path = "/disk/delete",
        .delete = disk_delete_handler
    },
    {
        .path = "/disk/list",
        .get = disk_list_get
    },
    {
        .path = "/disk/mkdir",
        .post = disk_mkdir_post
    },
    {
        .path = "/disk/rename",
        .post = disk_rename_post
    },
    {
        .path = "/notice",
        .get = notice_get
    },
    {
        .path = "/system",
        .get = system_get
    },
    {
        .path = "/photos",
        .get = photos_list_get,
        .post = photos_upload,    // POST → 上传新照片
        .delete = photos_delete // DELETE → 删除指定照片
    },
    {
        .path = "/photo",
        .get = photos_photo_get
    },
    {
        .path = "/photo/note",
        .get = photo_note_get,
        .post = photo_note_post
    },
    {
        .path = "/family/members",
        .get = family_members_get,
        .post = family_members_post
    },
    {
        .path = "/family/task",
        .post = family_task_post
    },
    {
        .path = "/family/my-tasks",
        .get = family_my_tasks_get
    },
    {
        .path = "/control/light",
        .get = control_light_get,
        .put = control_light_put,
    },
    {
        .path = "/control/aircon",
        .get = control_aircon_get,
        .put = control_aircon_put,
    },
    {
        .path = "/control/washing_machine",
        .get = control_washing_machine_get,
        .put = control_washing_machine_put,
    },
    {
        .path = "/control/fan",
        .get = control_fan_get,
        .put = control_fan_put,
    },
    {
        .path = "/control/door",
        .get = control_door_get,
        .put = control_door_put,
    },
    {
        .path = "/settings/change-password",
        .get = settings_change_password_get,
        .post = settings_change_password_post,
    },
    {
        .path = "/settings/public",
        .get = settings_public_get,
        .put = settings_public_put
    },
    {
        .path = "/settings/wifi",
        .get = settings_wifi_get,
        .put = settings_wifi_put
    },
    { NULL, NULL, NULL, NULL, NULL, NULL }
};