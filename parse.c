#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define IF_MODIFIED "If-Modified-Since"
#define IF_MODLEN   17
#define RFC1123DATE "%a, %d %b %Y %H:%M:%S GMT"
#define RFC850DATE  "%A, %d-%b-%y %H:%M:%S GMT"
#define ASCTIMEDATE "%a %b%t%d %H:%M:%S %Y"

void
invalidate_request(struct http_request *req)
{
	req->type = UNSUPPORTED;
	req->uri = NULL;
	req->time = NULL;
	req->if_modified = req->mjr = req->mnr = -1;
}

/* parse_request_type: read(2) upto 5 bytes from `fd`.  Check whether the
 * request type is supported.  Assumes that the size of the buffer is more
 * than 5 bytes.
 */
int
parse_request_type(int fd, struct http_request *req,
		   char *buf, size_t *len)
{
	int rd;

	if ((rd = read(fd, buf, 4)) == -1)
		err(1, "read");
	*len = rd;

	if ((strncmp(buf, "GET ", 4)) == 0) {
		req->type = GET;
		return 0;
	} else if ((strncmp(buf, "HEAD", 4)) == 0) {
		req->type = HEAD;
		if ((rd = read(fd, buf + *len, 1)) == -1)
			err(1, "read");
		*len += 1;
		return (*(buf + *len - 1) == ' ' ? 0 : -1);
	}
	return -1;
}

/* parse_uri_version: parse information from first line of request and
 * update `req`.
 */
int
parse_uri_version(int fd, struct http_request *req, char *buf,
		  size_t *len, size_t *size)
{
	size_t rd, spc;
	char *savebuf, *invalid;

	/* TODO: Read till CRLF not LF */
	while (1) {
		if ((strncmp(buf + *len - 2, "\r\n", 2)) == 0)
			break;
		if ((int)(rd = read(fd, buf + *len, *size - *len)) == -1)
			err(1, "read");
		if (rd == *size - *len) {
			*size *= 2;
			if ((buf = realloc(buf, *size)) == NULL)
				err(1, "realloc");
		} else if (rd == 0)
			break;
		*len += rd;
	}

	if ((savebuf = strsep(&buf, " ")) == NULL)
		return -1;

	spc = strcspn(buf, " ");

	(void)strncpy(req->uri, buf, spc);
	*(req->uri + spc + 1) = '\0';

	if (spc == strlen(buf)) { /* simple request */
		req->mjr = 0;
		req->mnr = 9;
		buf = savebuf;
		return 0;
	}
	
	if ((strsep(&buf, " ")) == NULL) {
		buf = savebuf;
		return -1;
	}
	
	if ((strncmp(buf,"HTTP",4)) != 0) {
		buf = savebuf;
		return -1;
	}

	if ((strsep(&buf, "/")) == NULL) {
		buf = savebuf;
		return -1;
	}

	req->mjr = strtol(buf, &invalid, 10);
	if (buf == invalid) {
		buf = savebuf;
		return -1;
	}

	if ((strsep(&buf, ".")) == NULL) {
		buf = savebuf;
		return -1;
	}
	
	req->mnr = strtol(buf, &invalid, 10);
	if (buf == invalid) {
		buf = savebuf;
		return -1;
	}
	
	buf = savebuf;
	return 0;
}

/* parse_headers: check for If-Modified-Since.  If not found, return 1;
 * if found, return 0;  if found, and date is invalid, return -1;
 */
int
parse_headers(struct http_request *req, char *buf)
{
	size_t spc;
	char *pnt, *chr;
	
	if ((strncmp(buf, IF_MODIFIED, IF_MODLEN)) != 0)
		return 1;

	req->if_modified = 1;

	spc = strcspn(buf, ":");
	spc += strspn(buf + spc + 1, " ");
	pnt = buf + spc + 1;

	if ((chr = strptime(pnt, RFC1123DATE, req->time)) != NULL)
		return 0;
	if ((chr = strptime(pnt, RFC850DATE, req->time)) != NULL)
		return 0;
	if ((chr = strptime(pnt, ASCTIMEDATE, req->time)) != NULL)
		return 0;
	
	return -1;
}

/* parse_request: parse http request and store information in `req`.  `req`
 * must be malloc'd by the caller.  It is assumed that `req.uri` contains
 * enough enough memory to store the URI.  Returns -1 on error, 0 otherwise.
 */
int
parse_request(int fd, struct http_request *req)
{
	size_t size, prev, len, rd;
	char *buf;

	size = BUFSIZ;
	len = 0;

	if ((buf = calloc(size, 1)) == NULL)
		err(1, "calloc");

	if ((parse_request_type(fd, req, buf, &len)) == -1) {
		invalidate_request(req);
		free(buf);
		return -1;
	}

	if ((parse_uri_version(fd, req, buf, &len, &size)) == -1) {
		invalidate_request(req);
		free(buf);
		return -1;
	}
	prev = len;
	
        /* parse request header fields */
	while (1) {
		if (strncmp(buf + len - 4, "\r\n\r\n", 4) == 0)
			break;
		if (strncmp(buf + len - 2, "\r\n", 2) == 0) {
			parse_headers(req, buf + prev);
			prev = len;
		}
		if ((int)(rd = read(fd, buf + len, size - len)) == -1)
			err(1, "read");
		if (rd == size - len) {
			size *= 2;
			if ((buf = realloc(buf, size)) == NULL)
				err(1, "realloc");
		} else if (rd == 0)
			break;
		len += rd;
	}

	free(buf);
	return 0;
}
