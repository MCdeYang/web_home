#include "control.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <json-c/json.h>  // ← 使用 json-c

// ----------------------------
// 辅助函数：使用 json-c 解析 state
// ----------------------------
int parse_state_from_json(const char *json_body) {
    if (!json_body) {
        return -1;
    }

    json_object *root = json_tokener_parse(json_body);
    if (!root) {
        fprintf(stderr, "JSON parse error: invalid syntax\n");
        return -1;
    }

    json_object *state_obj;
    int state = -1;

    // 检查是否包含 "state" 字段
    if (json_object_object_get_ex(root, "state", &state_obj)) {
        // 必须是整数类型（json-c 中数字默认为 int64）
        if (json_object_is_type(state_obj, json_type_int)) {
            int64_t val = json_object_get_int64(state_obj);
            if (val == 0 || val == 1) {
                state = (int)val;
            }
        }
        // 可选：支持布尔值 true/false
        // else if (json_object_is_type(state_obj, json_type_boolean)) {
        //     state = json_object_get_boolean(state_obj) ? 1 : 0;
        // }
    }

    json_object_put(root); // 释放内存
    return state; // 0, 1, or -1
}