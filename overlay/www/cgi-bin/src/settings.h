#ifndef SETTINGS_H
#define SETTINGS_H

#define LOGIN_FILE      "/development/login_info/login.txt"
#define PUBLIC_FILE     "/development/login_info/public_enabled.txt"

void parse_json_credentials(const char *body, char *username, char *password, size_t max_len);
int is_ngrok_running();
void read_username(char *username, size_t size);
int write_public_file(int enabled);
int write_login_file(const char *username, const char *password);
void json_escape(const char *src, char *dst, size_t dst_size);
void extract_value(const char *line, const char *key, char *out, size_t out_size);
//void send_json_response(int status_code, const char *status_msg, const char *json);
int load_stored_credentials(char *stored_user, size_t user_size,char *stored_pass, size_t pass_size);

#endif // SETTINGS_H