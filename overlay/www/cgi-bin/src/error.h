#ifndef ERROR_H
#define ERROR_H
void json_success(const char *msg);
void send_error_400(const char *message);
void send_error_401(const char *message);
void send_error_403(const char *message);
void send_error_404(const char *message);
void send_error_405(const char *message);
void send_error_500(const char *message);

// 通用错误发送函数
void _send_json_error(int status_code, const char *status_text, const char *message);

#endif