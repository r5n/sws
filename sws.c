#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define PORT 8080

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
        if (clientsock < 0)
            perror("accept");

        switch (fork()) {
            case -1:
                err(1, "fork");
                break;

            case 0: // child
                printf("got connection from %s\n",
                        inet_ntoa(client.sin_addr));
                break;

            default: // parent
                printf("accepted\n");
                break;
        }
    }
}
