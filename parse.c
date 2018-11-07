#include <err.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define VERSION_MINLEN 8

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

	/* DEBUG */
	if (req->type == GET) printf("GET");
	if (req->type == HEAD) printf("HEAD");
	if (req->type == UNSUPPORTED) printf("UNSUPPORTED");
	printf("\turi: %s", req->uri);
	printf("\tver: %d:%d\n", req->mjr, req->mnr);
}

int
main(void)
{
	struct http_request req;
	ssize_t rd, size, len;
	char *buf;

	size = BUFSIZ;
	len = 0;
	if ((buf = calloc(size, 1)) == NULL)
		err(1, "calloc");

	while (1) {
		if (strncmp(buf + len - 1, "\n", 1) == 0)
			break;
		if ((rd = read(STDIN_FILENO, buf, size - len)) == -1)
			err(1, "read");
		if (rd == size - len) {
			size *= 2;
			if ((buf = realloc(buf, size)) == NULL)
				err(1, "realloc");
		} else if (rd == 0)
			break;
		len += rd;			
	}

	if ((req.uri = malloc(sizeof(char *) * PATH_MAX)) == NULL) {
		err(1, "malloc");
	}

	buf[++len] = '\0';
	parse_header(&req, PATH_MAX, buf, len);

	free(req.uri);
	free(buf);
	return 0;
}
