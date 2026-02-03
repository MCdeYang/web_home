/**********************************************************************
 * @file check_task.c
 * @brief 任务检查线程实现
 *
 * 本文件实现了通过检查任务 API 定期获取当前任务信息，
 * 
 * @author 杨翊
 * @date 2026-02-03
 * @version 1.0
 *
 * @note
 * - 缓存文件路径为 /development/tmp/weather.json，确保目录存在且可写。
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <json-c/json.h>
#include "check_task.h"
#include "define.h"

// 简单哈希函数
static unsigned int simple_hash(const char *str) {
    unsigned int hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}
// 生成任务ID
static char* make_task_id(json_object *task) {
    const char *title;
    const char *due_date;
    const char *creator;
    char buf[512];
    static char id[16];

    title = json_object_get_string(
        json_object_object_get(task, "title")
    );
    due_date = json_object_get_string(
        json_object_object_get(task, "due_date")
    );
    creator = json_object_get_string(
        json_object_object_get(task, "creator")
    );
    snprintf(buf, sizeof(buf), "%s|%s|%s",
        title ? title : "",
        due_date ? due_date : "",
        creator ? creator : ""
    );// 组合任务标题、到期日期和创建人
    snprintf(id, sizeof(id), "%u", simple_hash(buf));// 对组合字符串进行哈希计算
    return id;
}
// 检查任务是否已通知
static int is_task_notified(const char *task_id) {
    FILE *fp;
    char line[64];
    size_t len;

    fp = fopen(STATE_FILE, "r");
    if (!fp) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp)) {
        len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        if (strcmp(line, task_id) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}
// 标记任务为已通知
static void mark_task_notified(const char *task_id) {
    FILE *fp;
    
    fp = fopen(STATE_FILE, "a");
    if (fp) {
        fprintf(fp, "%s\n", task_id);
        fclose(fp);
    }
}
// 发送通知
static void push_notification(const char *sckey, const char *title, const char *desp) {
    char cmd[1024];

    if (!sckey || !title || !desp) return;
    
    snprintf(cmd, sizeof(cmd),
        "curl -k -m 10 -s -o /dev/null "
        "-d 'title=%s' "
        "-d 'desp=%s' "
        "'https://sctapi.ftqq.com/%s.send'",
        title, desp, sckey
    );
    system(cmd);
}
// 解析日期时间字符串
static time_t parse_datetime(const char *dt_str) {
    struct tm tm = {0};
    int year, mon, mday, hour, min;

    if (!dt_str) return -1;
    
    if (sscanf(dt_str, "%d-%d-%d %d:%d", &year, &mon, &mday, &hour, &min) != 5) {
        return -1;
    }
    tm.tm_year = year - 1900;
    tm.tm_mon  = mon - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    tm.tm_min  = min;
    tm.tm_isdst = -1;
    return mktime(&tm);
}
// 检查任务
static void check_once() {
    FILE *fp;
    long size;
    char *buf;
    json_object *root;
    time_t now;
    json_object *members;
    int i, j;
    int members_len, tasks_len;
    json_object *member;
    const char *wechat_id;
    json_object *tasks;
    json_object *task;
    const char *due_str;
    time_t due_time;
    char *task_id;
    const char *title;
    const char *creator;
    char desp[512];

    fp = fopen(DATA_FILE, "r");
    if (!fp) {
        return;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return;
    }
    fseek(fp, 0, SEEK_SET);
    buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return;
    }
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    root = json_tokener_parse(buf);
    free(buf);
    if (!root) {
        return;
    }
    now = time(NULL);
    members = json_object_object_get(root, "members");
    if (members && json_object_is_type(members, json_type_array)) {
        members_len = json_object_array_length(members);
        for (i = 0; i < members_len; i++) {
            member = json_object_array_get_idx(members, i);
            wechat_id = json_object_get_string(
                json_object_object_get(member, "wechat_id")
            );
            tasks = json_object_object_get(member, "tasks");
            if (!tasks || !json_object_is_type(tasks, json_type_array)){ 
                continue;
            }
            tasks_len = json_object_array_length(tasks);
            for (j = 0; j < tasks_len; j++) {
                task = json_object_array_get_idx(tasks, j);
                due_str = json_object_get_string(
                    json_object_object_get(task, "due_date")
                );
                due_time = parse_datetime(due_str);
                if (due_time == -1 || due_time > now) {
                    continue;
                }
                task_id = make_task_id(task);
                if (is_task_notified(task_id)) {
                    continue;
                }
                title = json_object_get_string(
                    json_object_object_get(task, "title")
                );
                creator = json_object_get_string(
                    json_object_object_get(task, "creator")
                );
                snprintf(desp, sizeof(desp),
                    "任务已到期！\n %s\n👤 %s\n %s",
                    title ? title : "(无标题)",
                    creator ? creator : "家人",
                    due_str ? due_str : ""
                );// 构建通知描述
                if (wechat_id && strlen(wechat_id) >= 3 && strncmp(wechat_id, "SCT", 3) == 0) {
                    push_notification(wechat_id, "【家庭任务】到期提醒！", desp);
                }
                mark_task_notified(task_id);
            }
        }
    }
    json_object_put(root);
}
// 检查任务线程
void *check_task_thread(void *arg) {
    printf("check_tasks daemon thread started.\n");
    printf("Data file: %s\n", DATA_FILE);
    printf("State file: %s\n", STATE_FILE);
    printf("Checking every %d seconds.\n", CHECK_INTERVAL);

    while (1) {
        check_once();
        sleep(CHECK_INTERVAL);
    }
    return NULL;
}
