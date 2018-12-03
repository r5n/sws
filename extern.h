#ifndef _EXTERN_H_
#define _EXTERN_H_

#include <stdbool.h>

struct options{
    bool cgi; 
    bool debug;
    bool help;
    bool bind_to;
    bool log;
    bool port;
};

struct server_info{
    char * cgi_dir;
    char * dir;
    char * address;
    char * logdir;
    char * port;
};

struct http_request {
    struct tm *time;
    enum {GET, HEAD, UNSUPPORTED} type;
    char *uri;
    int if_modified;
    int mjr;
    int mnr;
};

typedef struct {
    struct tm *last_modified;
    char *content;
    char *content_type;
    int code;
} response;

int parse_request(int, struct http_request *);
int parse_args(int, char **,struct options *,struct server_info *);
void do_cgi(char *, response *);
void handle_request(int, struct options *,
                    struct server_info *, struct http_request *, char *);
void listing(int, char *, struct tm *, response *);
void respond(int, response *);
void internal_error(int);
void listing(int, char *, struct http_request *, struct http_response *);

#endif // ifndef _EXTERN_H_
