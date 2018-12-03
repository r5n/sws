#ifndef _EXTERN_H_
#define _EXTERN_H_

#include <stdbool.h>
#include <sys/socket.h>

struct server_info {
    char *cgi_dir;
    char *dir;
    char *address;
    char *port;
    char *logdir;
    bool debug;
};

struct http_request {
    struct tm *time;
    enum { GET, HEAD, UNSUPPORTED } type;
    char *uri;
    int if_modified;
    int mjr;
    int mnr;
    struct sockaddr *addr;
    socklen_t addrlen;
};

typedef struct {
    struct tm *last_modified;
    char *content;
    long long content_len;
    char *content_type;
    int code;
} response;

extern char *convert_to_string[];

int parse_request(int, struct http_request *);
int parse_args(int, char **, struct server_info *);
void do_cgi(char *, struct server_info *, struct http_request *, response *);
void handle_request(int, struct server_info *, struct http_request *, char *);
void respond(int, struct http_request *, response *);
void internal_error(int, struct http_request *);
void listing(int, char *, struct http_request *, response *);
void logging(struct http_request *, char *, response *);

#endif // ifndef _EXTERN_H_
