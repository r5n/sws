#ifndef _EXTERN_H_
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
    char * dir;
    char * address;
    char * logdir;
    char * port;
};

int parse_args(int, char **,struct options *,struct server_info *);
#endif

