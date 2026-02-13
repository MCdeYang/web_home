#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "routes.h"
#include "error.h"
#include "token.h"

char*read_stdin_body(void);
int authenticate_request(const char*path_info);
int dispatch_route(const char*method,const char*path_info,const char*body);

char*read_stdin_body(void){
    char*clen=getenv("CONTENT_LENGTH");
    if(!clen)return NULL;
    long len=atol(clen);
    if(len<=0||len>16384)return NULL;

    char*buf=malloc(len+1);
    if(!buf)return NULL;
    if(fread(buf,1,len,stdin)!=(size_t)len){
        free(buf);
        return NULL;
    }
    buf[len]='\0';
    return buf;
}

int authenticate_request(const char*path_info){
    if(strcmp(path_info,"/login")==0){
        return 1;
    }

    char*token=get_token_from_cookie();
    int valid=is_valid_token(token);
    free(token);

    if(!valid){
        printf("Status: 401 Unauthorized\r\n");
        printf("Content-Type: application/json\r\n\r\n");
        printf("{\"error\":\"Unauthorized\",\"redirect\":\"/login.html\"}\n");
        return 0;
    }
    return 1;
}

int dispatch_route(const char*method,const char*path_info,const char*body){
    int i;
    for(i=0;routes[i].path;i++){
        if(strcmp(routes[i].path,path_info)==0){
            void(*handler)(const char*,const char*)=NULL;

            if(strcmp(method,"GET")==0){
                handler=routes[i].get;
            }else if(strcmp(method,"POST")==0){
                handler=routes[i].post;
            }else if(strcmp(method,"PUT")==0){
                handler=routes[i].put;
            }else if(strcmp(method,"PATCH")==0){
                handler=routes[i].patch;
            }else if(strcmp(method,"DELETE")==0){
                handler=routes[i].delete;
            }

            if(handler){
                handler(path_info,body);
                fflush(stdout);
                return 1;
            }else{
                send_error_405("Only GET and POST are allowed");
                fflush(stdout);
                return 0;
            }
        }
    }
    return 0;
}

int main(void){
    char*method=getenv("REQUEST_METHOD");
    char*path_info=getenv("PATH_INFO");
    if(!method)method="GET";
    if(!path_info)path_info="/";

    char*body=NULL;
    if(strcmp(method,"GET")!=0&&strcmp(method,"HEAD")!=0){
        body=read_stdin_body();
    }

    if(!authenticate_request(path_info)){
        if(body)free(body);
        return EXIT_FAILURE;
    }

    if(!dispatch_route(method,path_info,body)){
        send_error_404("YY tell you 404 error");
        fflush(stdout);
    }

    if(body)free(body);
    return EXIT_SUCCESS;
}

