#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void http(int fd) {
    char *buf;
    ssize_t rd, size, length;

    size = BUFSIZ;
    length = 0;
    buf = calloc(size, 1);

    while (true) {
        if (strncmp(buf + length - 4, "\r\n\r\n", 4) == 0) {
            break;
        }

        rd = read(fd, buf, size - length);
        if (rd == size - length) {
            size *= 2;
            buf = realloc(buf, size);
        } else if (rd == 0) {
            printf("read 0\n");
            break;
        }
        length += rd;
    }

    printf("received [%s]\n", buf);

    free(buf);
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

int main() {
    struct sockaddr_in6 client; // assuming sockaddr_in6 is bigger than sockaddr_in
    struct addrinfo hints, *res;
    int clientsock, set, gaierr, nsocks, i;
    socklen_t clientsz;
    char *host, *port;
    struct pollfd *fds;

    host = NULL;
    port = "8080";
    nsocks = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;

    if ((gaierr = getaddrinfo(host, port, &hints, &res)) != 0)
        errx(1, "getaddrinfo([%s], [%s]): %s", host, port, gai_strerror(gaierr));

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

        char *ipport = sockaddr_to_str(addr->ai_addr, addr->ai_addrlen);
        printf("Listening on %s\n", ipport);
        free(ipport);

        ++i;
    }

    freeaddrinfo(res);

    while (poll(fds, nsocks, 0) > 0) {
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

                case 0: { // child
                    char *ipport = sockaddr_to_str((struct sockaddr *)&client, clientsz);
                    printf("got connection from %s\n", ipport);
                    free(ipport);
                    http(clientsock);
                    break;
                }

                default: // parent
                    printf("accepted\n");
                    break;
            }
        }
    }
}
