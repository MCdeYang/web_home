#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <json-c/json.h>
#include "check_task.h"

#define DATA_FILE    "/development/tmp/members.json"
#define STATE_FILE   "/development/tmp/check_tasks.state"
#define CHECK_INTERVAL 30

static unsigned int simple_hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static char* make_task_id(json_object *task) {
    const char *title = json_object_get_string(
        json_object_object_get(task, "title")
    );
    const char *due_date = json_object_get_string(
        json_object_object_get(task, "due_date")
    );
    const char *creator = json_object_get_string(
        json_object_object_get(task, "creator")
    );

    char buf[512];
    snprintf(buf, sizeof(buf), "%s|%s|%s",
        title ? title : "",
        due_date ? due_date : "",
        creator ? creator : ""
    );

    static char id[16];
    snprintf(id, sizeof(id), "%u", simple_hash(buf));
    return id;
}

static int is_task_notified(const char *task_id) {
    FILE *fp = fopen(STATE_FILE, "r");
    if (!fp) return 0;

    char line[64];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
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

static void mark_task_notified(const char *task_id) {
    FILE *fp = fopen(STATE_FILE, "a");
    if (fp) {
        fprintf(fp, "%s\n", task_id);
        fclose(fp);
    }
}

static void push_notification(const char *sckey, const char *title, const char *desp) {
    if (!sckey || !title || !desp) return;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -k -m 10 -s -o /dev/null "
        "-d 'title=%s' "
        "-d 'desp=%s' "
        "'https://sctapi.ftqq.com/%s.send'",
        title, desp, sckey
    );
    system(cmd);
}

static time_t parse_datetime(const char *dt_str) {
    if (!dt_str) return -1;
    struct tm tm = {0};
    int year, mon, mday, hour, min;
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

static void check_once() {
    FILE *fp = fopen(DATA_FILE, "r");
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return;
    }
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return;
    }
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);

    json_object *root = json_tokener_parse(buf);
    free(buf);
    if (!root) return;

    time_t now = time(NULL);

    json_object *members = json_object_object_get(root, "members");
    if (members && json_object_is_type(members, json_type_array)) {
        for (int i = 0; i < json_object_array_length(members); i++) {
            json_object *member = json_object_array_get_idx(members, i);
            const char *wechat_id = json_object_get_string(
                json_object_object_get(member, "wechat_id")
            );

            json_object *tasks = json_object_object_get(member, "tasks");
            if (!tasks || !json_object_is_type(tasks, json_type_array)) continue;

            for (int j = 0; j < json_object_array_length(tasks); j++) {
                json_object *task = json_object_array_get_idx(tasks, j);

                const char *due_str = json_object_get_string(
                    json_object_object_get(task, "due_date")
                );
                time_t due_time = parse_datetime(due_str);

                if (due_time == -1 || due_time > now) {
                    continue;
                }

                char *task_id = make_task_id(task);
                if (is_task_notified(task_id)) {
                    continue;
                }

                const char *title = json_object_get_string(
                    json_object_object_get(task, "title")
                );
                const char *creator = json_object_get_string(
                    json_object_object_get(task, "creator")
                );

                char desp[512];
                snprintf(desp, sizeof(desp),
                    "⏰ 任务已到期！\n📝 %s\n👤 %s\n📆 %s",
                    title ? title : "(无标题)",
                    creator ? creator : "家人",
                    due_str ? due_str : ""
                );

                if (wechat_id && strlen(wechat_id) >= 3 && strncmp(wechat_id, "SCT", 3) == 0) {
                    push_notification(wechat_id, "【家庭任务】到期提醒！", desp);
                }

                mark_task_notified(task_id);
            }
        }
    }

    json_object_put(root);
}

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