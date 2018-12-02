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

int parse_request(int, struct http_request *);
int parse_args(int, char **,struct options *,struct server_info *);

#endif // ifndef _EXTERN_H_
