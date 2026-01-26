// token.h
#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

#define TOKEN_LEN 32

void generate_token(char *token, size_t len);
int add_token(const char *token);
int is_valid_token(const char *token);
char* get_token_from_cookie(void);
void clear_all_tokens(void);

#endif // TOKEN_H