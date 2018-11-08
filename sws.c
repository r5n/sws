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

struct http_request {
	enum {GET, HEAD, UNSUPPORTED} type;
	char *uri;
	int if_modified;
	int mjr;
	int mnr;
};

void
invalidate_request(struct http_request *req)
{
	req->type = UNSUPPORTED;
	req->uri = NULL;
	req->if_modified = -1;
	req->mjr = -1;
	req->mnr = -1;
}

void
parse_header(struct http_request *req, ssize_t urisz, char *buf, ssize_t bufsz)
{
	size_t len;
	char *invalid;

	if (bufsz == 0) {
		invalidate_request(req);
		return;
	}

	len = strcspn(buf, " ");
	if ((strncmp(buf, "GET", len)) == 0)
		req->type = GET;
	else if ((strncmp(buf, "HEAD", len)) == 0)
		req->type = HEAD;
	else
		req->type = UNSUPPORTED;

	buf += len + 1;
	len = strcspn(buf, " ");

	strncpy(req->uri, (const char *)buf, urisz);
	req->uri[len] = '\0';

	buf += len + 1;
	len = strcspn(buf, "/");

	if ((strncmp(buf, "HTTP", len)) != 0) {
		invalidate_request(req);
		return;
	}

	buf += len + 1;
	len = strcspn(buf, "\r\n");

	if (len == 0) {
		req->mjr = 0;
		req->mnr = 9;
		return;
	}
	
	len = strcspn(buf, ".");
	req->mjr = strtol(buf, &invalid, 10);

	if (buf == invalid) {
		invalidate_request(req);
		return;
	}
	
	buf += len + 1;
	req->mnr = strtol(buf, &invalid, 10);

	if (buf == invalid) {
		invalidate_request(req);
		return;
	}
}

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
