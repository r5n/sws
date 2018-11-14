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

#include "extern.h"

static void usage();
void assert_valid_address(char *ipAddress);
void assert_valid_port(char *port_string);

int
parse_args(int argc, char **argv,struct options *options,
        struct server_info * server_info)
{ 
    int c;
    char *host_name;
    setprogname(argv[0]);
    while((c = getopt (argc,argv, "c:dhi:l:p:")) != -1)
        switch(c)
        {
            case 'c':
                options->cgi = true;
                server_info->dir = optarg;
                break;
            case 'd':
                options->debug = true;
                break;
            case 'h':
                options->help = true;
                usage();
                return 1;
            case 'i':
                options->bind_to = true;
                host_name = optarg;
                assert_valid_address(host_name);
                server_info->address = host_name;
                break;
            case 'l':
                options->log = true;
                server_info->logdir = optarg;
                break;
            case 'p':
                options->port = true;
                assert_valid_port(optarg);
                server_info->port = optarg;
                break;
            default:
                usage();
                return 1;
        }
    return 0;
}


void
assert_valid_address(char *ipAddress){
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    int result_ipv4, result_ipv6;
    
    if((strnlen(ipAddress, INET_ADDRSTRLEN) == INET_ADDRSTRLEN) &&
        INET6_ADDRSTRLEN == strnlen(ipAddress, INET6_ADDRSTRLEN)){
        errx(EXIT_FAILURE, "Address is too long");
    }
    
    if((result_ipv4 = inet_pton(AF_INET, ipAddress, &(sa.sin_addr))) == -1){
        err(EXIT_FAILURE, "Could not parse provided IPv4 address");
    }
    
    if((result_ipv6 = inet_pton(AF_INET6, ipAddress, &(sa6.sin6_addr))) == -1){
        err(EXIT_FAILURE, "Could not parse provided IPv6 address");
    }
    
    if(!result_ipv4 && !result_ipv6){
        errx(EXIT_FAILURE, "Please provide a suitable address");
    }
}

void
assert_valid_port(char *port_string)
{
    char *ep;
    long lval;

    errno = 0;
    
    lval = strtol(port_string, &ep, 10);
    
    if (ep == port_string || *ep != '\0'){
        errx(EXIT_FAILURE, "Please enter a valid port number");
    }

    if (errno == ERANGE || lval <= 0 || 65535 < lval){
        errx(EXIT_FAILURE, "Port number is out of range");
    }

    if(errno){
        err(EXIT_FAILURE, "An error occured while parsing the provided port");
    }

}

static void
usage(void){
    (void)fprintf(stderr,"usage: %s [-dh] [-c dir] [-i address] [-l file] [-p port] dir\n",getprogname()); 
}
