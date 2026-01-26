#ifndef _ROUTES_H
#define _ROUTES_H

typedef void (*route_handler_t)(const char *path, const char *body);

struct route {
    const char *path;
    route_handler_t get;
    route_handler_t post;
    route_handler_t put;
    route_handler_t patch;
    route_handler_t delete;
};
extern struct route routes[];
#endif