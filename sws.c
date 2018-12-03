#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "extern.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct server_info server_info;
int logfd;

#define LOG_PERMS 0600
#define LOG_MODES O_WRONLY | O_APPEND | O_CREAT

char *
convert_to_string[] = {
        "GET","HEAD","UNSUPPORTED"
};

// log format: remote_ip time_in_GMT first_line status size_of_the_response
void
logging(struct http_request *req, char *line, response *resp)
{
        time_t now;
        char time_line[BUFSIZ];
        char buf[BUFSIZ];
        const char *ip;
        struct tm *time_struct;

        if (time(&now) == -1)
                err(1, "time");
        time_struct = gmtime(&now);

        if ((ip = inet_ntop(req->addr->sa_family, req->addr, buf, *req->addrlen)) == NULL) 
            err(1, "inet_pton");

        if(strftime(time_line,BUFSIZ,"%d/%b/%Y:%T:%z",time_struct) == 0){
                err(1,"Log time formatting");
        }

        if (sprintf(buf, "%s %s \"%s\" %d %lu\n",
                    ip, time_line, line, resp->code, strlen(resp->content)) < 0)
                err(1, "sprintf");

        if (server_info.debug) {
                printf("%s", buf);
        } else {
                if (write(logfd, buf, strlen(buf)) == -1)
                        err(1,"Could not write %s to file:",line);
        }
}

void http(int fd, char *cwd,
          struct sockaddr *addr, socklen_t *addrlen)
{
    struct http_request req;

    if ((req.time = malloc(sizeof(struct tm))) == NULL)
        err(1, "malloc");

    req.addr = addr;
    req.addrlen = addrlen;

    if (parse_request(fd, &req) == -1) {
        respond(fd, &req, &(response){
                .content = NULL,
                .code = 400,
                .last_modified = NULL,
        });
        return;
    }

    handle_request(fd, &server_info, &req, cwd);
}

char *sockaddr_to_str(struct sockaddr *addr, socklen_t addrlen) {
	char nihost[NI_MAXHOST] = {0};
	char niserv[NI_MAXSERV] = {0};
	char *res;
	int len, gaierr;

	// we exit in this utility function, because if we cannot convert an IPv4
	// or IPv6 addr, something is really wrong
	if ((gaierr = getnameinfo(addr, addrlen,
					nihost, sizeof nihost,
					niserv, sizeof niserv,
					NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
		errx(1, "getnameinfo(): %s", gai_strerror(gaierr));

	// format: [host]:port
	len = 1 + strlen(nihost) + 2 + strlen(niserv) + 1;
	if ((res = calloc(1, len)) == NULL)
		err(1, "calloc");

	if (addr->sa_family == AF_INET6)
		snprintf(res, len, "[%s]:%s", nihost, niserv);
	else
		snprintf(res, len, "%s:%s", nihost, niserv);

	return res;
}

void
init_settings(){
    server_info.cgi_dir = NULL;
    server_info.dir = NULL;
    server_info.address = NULL;
    server_info.logdir = NULL;
    server_info.port = "8080";
    server_info.debug = true;
}

int 
main(int argc,char **argv) {
    struct sockaddr_in6 client; // assuming sockaddr_in6 is bigger than sockaddr_in
    struct addrinfo hints, *res;
    int clientsock, set, status;
    nfds_t nsocks, i;
    socklen_t clientsz;
    char *host, *port, *ipport, *dir;
    struct pollfd *fds;
    struct stat dir_st;

    init_settings();
    if (parse_args(argc, argv, &server_info))
        return 1;

    dir = realpath(server_info.dir, NULL);
    if (!dir)
        err(1, "web dir: %s", server_info.dir);

    if (stat(dir, &dir_st) < 0)
        err(1, "web dir: %s", dir);

    if (!S_ISDIR(dir_st.st_mode))
        errx(1, "%s is not a directory", dir);

    printf("Serving from %s\n", dir);
    
    host = server_info.address;
    port = server_info.port;
    nsocks = 0;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;

    if (!server_info.debug && (logfd = open(server_info.logdir, LOG_MODES, LOG_PERMS)) == -1)
	err(1,"Could not open log file");

    if ((status = getaddrinfo(host, port, &hints, &res)) != 0)
        errx(1, "getaddrinfo([%s], [%s]): %s", host, port, gai_strerror(status));

    for (struct addrinfo *addr = res; addr; addr = addr->ai_next)
        ++nsocks;

    if ((fds = calloc(nsocks, sizeof(struct pollfd))) == NULL)
        err(1, "calloc");

    i = 0;
    for (struct addrinfo *addr = res; addr; addr = addr->ai_next) {
        fds[i].events = POLLIN;
        fds[i].fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fds[i].fd < 0)
            err(1, "socket");

        set = 1;
        if (addr->ai_family == AF_INET6)
            // disable dual-stack sockets, so that we can bind to the same port on IPv4
            if (setsockopt(fds[i].fd, IPPROTO_IPV6, IPV6_V6ONLY, &set, sizeof set) < 0)
                err(1, "setsockopt(IPV6_V6ONLY)");

        set = 1;
        if (setsockopt(fds[i].fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof set) < 0)
            perror("setsockopt(SO_REUSEADDR)");

        if (bind(fds[i].fd, addr->ai_addr, addr->ai_addrlen) < 0)
            err(1, "bind");

        if (listen(fds[i].fd, 5) < 0) // TODO find a good value
            err(1, "listen");

        ipport = sockaddr_to_str(addr->ai_addr, addr->ai_addrlen);
        printf("Listening on %s\n", ipport);
        free(ipport);

        ++i;
    }

    freeaddrinfo(res);

    while ((status = poll(fds, nsocks, -1)) > 0) {
        for (i = 0; i < nsocks; ++i) {
            if (!fds[i].revents)
                continue;

            clientsz = sizeof(client);
            clientsock = accept(fds[i].fd, (struct sockaddr *)&client, &clientsz);
            if (clientsock < 0) {
                perror("accept");
                continue;
            }

            switch (fork()) {
                case -1:
                    err(1, "fork");
                    break;

                case 0: // child
                    http(clientsock, dir,
                         (struct sockaddr*)&client, &clientsz);
                    break;

                default: // parent
                    break;
            }
        }
    }
    if (status < 0)
        err(1, "poll");
    
    if (!server_info.debug)
        (void)close(logfd);

    printf("Exiting\n");
}
