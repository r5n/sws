#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static void usage();
void is_valid_ip(char *ipAddress);
/*
 * Source: strtol(3)
 */
int convert_to_int(char *port_string);


struct options{
    bool cgi; 
    bool debug;
    bool help;
    bool bind_to;
    bool log;
    bool port;
}options;

int
main(int argc, char **argv)
{ 
    int c;
    int port_num;
    char *host_name;
    char * cgi_dir;
    setprogname(argv[0]);
    while((c = getopt (argc,argv, "c:dhi:l:p:")) != -1)
        switch(c)
        {
            case 'c':
                options.cgi = true;
                if(!optarg)
                    usage();
                cgi_dir = optarg;
                printf("dir: %s\n",cgi_dir);
                break;
            case 'd':
                options.debug = true;
                break;
            case 'h':
                options.help = true;
                usage();
                break;
            case 'i':
                options.bind_to = true;
                host_name = optarg;
                is_valid_ip(host_name);
                printf("%s\n",host_name);
                break;
            case 'l':
                options.log = true;
                break;
            case 'p':
                options.port = true;
                port_num = convert_to_int(optarg); 
                printf("port: %d\n",port_num);
                break;
            default:
                usage();
                break;
        }
    exit(EXIT_SUCCESS);
}


void
is_valid_ip(char *ipAddress){
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    int result_ipv4, result_ipv6;
    
    if((strnlen(ipAddress, INET_ADDRSTRLEN) == INET_ADDRSTRLEN) && \
        INET6_ADDRSTRLEN == strnlen(ipAddress, INET6_ADDRSTRLEN)){
        fprintf(stderr,"address is too long\n");
        exit(EXIT_FAILURE);
    }
    
    if((result_ipv4 = inet_pton(AF_INET, ipAddress, &(sa.sin_addr))) == -1){
            fprintf(stderr,"Could not parse provided address\n");
            exit(EXIT_FAILURE);
    }
    
    if((result_ipv6 = inet_pton(AF_INET6, ipAddress, &(sa6.sin6_addr))) == -1){
            fprintf(stderr,"Could not parse provided address\n");
            exit(EXIT_FAILURE);
    }
    
    if(!result_ipv4 && !result_ipv6){
        fprintf(stderr,"Please provide a suitable address\n");
        exit(EXIT_FAILURE);
    }
}

int
convert_to_int(char *port_string)
{
    char *ep;
    int ival;
    long lval;
    errno = 0;
    
    lval = strtol(port_string, &ep, 10);
    
    if (ep == port_string || *ep != '\0'){
        fprintf(stderr,"Please enter a valid port number\n");
        exit(EXIT_FAILURE);
    }

    if (errno == ERANGE || lval < INT_MIN || INT_MAX < lval){
        fprintf(stderr,"Port number is out of range\n");
        exit(EXIT_FAILURE);
    }

    if(!errno == 0){
        fprintf(stderr,"An error occured while parsing the provided port: %s\n",strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(INT_MIN > lval || lval > INT_MAX){
        fprintf(stderr,"Port number is too large \n");
        exit(EXIT_FAILURE);
    }

    ival = lval;
    return ival;
}

static void
usage(void){
    (void)fprintf(stderr,"usage: %s [-dh] [-c dir] [-i address] [-l file] [-p port] dir\n",getprogname()); 
    if(!options.help)
        exit(EX_USAGE);
}
