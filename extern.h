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

struct http_response {
    struct tm *last_modified;
    char *content_type;
    char *reason;
    size_t content_length;
    int status;
};

int parse_request(int, struct http_request *);
int parse_args(int, char **,struct options *,struct server_info *);
void write_bad_request(int);
void cgi(int, char *);
void handle_request(int, struct options *,
		    struct server_info *, struct http_request *);
void listing(int, char *, struct tm *);

#endif // ifndef _EXTERN_H_
