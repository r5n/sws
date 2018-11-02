#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT 8080

void http(int fd, struct sockaddr_in addr) {
    (void)addr;
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

int main() {
    struct sockaddr_in server, client;
    int serversock, clientsock;
    socklen_t clientsz;
    int on;

    on = 1;

    serversock = socket(PF_INET, SOCK_STREAM, 0);
    if (serversock < 0)
        err(1, "socket");

    server = (struct sockaddr_in) {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (setsockopt(serversock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0)
        perror("setsockopt(SO_REUSEADDR)");

    if (bind(serversock, (struct sockaddr *)&server, sizeof(server)) < 0)
        err(1, "bind(port = %d)", PORT);

    listen(serversock, 5); // TODO find a good value

    while (true) {
        clientsz = sizeof(client);
        clientsock = accept(serversock, (struct sockaddr *)&client, &clientsz);
        if (clientsock < 0) {
            perror("accept");
            continue;
        }

        switch (fork()) {
            case -1:
                err(1, "fork");
                break;

            case 0: // child
                printf("got connection from %s\n",
                        inet_ntoa(client.sin_addr));
                http(clientsock, client);
                break;

            default: // parent
                printf("accepted\n");
                break;
        }
    }
}
