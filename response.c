#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define BAD_REQ      "Bad Request"
#define BAD_REQ_LEN  11
#define CGI_BIN      "/cgi-bin"
#define CRLF         "\r\n"
#define DATE_FMT     "%a, %d %b %Y %H:%M:%S GMT"
#define HTTP_TYPE    "HTTP/1.0"
#define SERVER_INFO  "sws/1.0"
#define TEXT         "something something dark side"
#define PLAIN_STR    "text/plain"
#define PLAIN_LEN    10

void
free_response(struct http_response *resp)
{
    free(resp->last_modified);
    free(resp->content_type);
    free(resp->reason);
    free(resp);
}

void
resp_header(int fd, struct http_response *resp)
{
    time_t now;
    struct tm *tm_p;
    char tmbuf[BUFSIZ], lmbuf[BUFSIZ];

    time(&now);
    tm_p = gmtime(&now);
    if (strftime(tmbuf, BUFSIZ, DATE_FMT, tm_p) == 0)
	err(1, "strftime");

    if (strftime(lmbuf, BUFSIZ, DATE_FMT, resp->last_modified) == 0)
	err(1, "strftime");

    dprintf(fd, "%s %d %s" CRLF, HTTP_TYPE, resp->status, resp->reason);
    dprintf(fd, "Date: %s" CRLF, tmbuf);
    dprintf(fd, "Server: %s" CRLF, SERVER_INFO);
    dprintf(fd, "Last-Modified: %s" CRLF, lmbuf);
    dprintf(fd, "Content-Type: %s" CRLF, resp->content_type);
    dprintf(fd, "Content-Length: %zu" CRLF, resp->content_length);
    dprintf(fd, "%s", CRLF);
}

/* For when parsing fails and we don't have a request object */
void
write_bad_request(int fd)
{
    struct http_response *resp;
    time_t now;
    struct tm *tm_p;
    
    time(&now);
    tm_p = gmtime(&now);

    /* Do we need last_modified? */
    if ((resp = malloc(sizeof(struct http_response))) == NULL)
	err(1, "malloc");
    
    resp->last_modified = tm_p;
    resp->status = 400;
    resp->content_length = strlen(BAD_REQ);

    if ((resp->content_type = malloc(PLAIN_LEN+1)) == NULL)
	err(1, "malloc");
    (void)strncpy(resp->content_type, "text/plain", PLAIN_LEN);
    resp->content_type[PLAIN_LEN+1] = '\0';

    if ((resp->reason = malloc(BAD_REQ_LEN+1)) == NULL)
	err(1, "malloc");
    (void)strncpy(resp->reason, BAD_REQ, BAD_REQ_LEN);
    resp->reason[BAD_REQ_LEN+1] = '\0';

    resp_header(fd, resp);
    dprintf(fd, "%s" CRLF, BAD_REQ);   /* For now */
    free_response(resp);
}

void
handle_request(int client, struct options *opt,
	       struct server_info *info, struct http_request *req)
{
    char *uri, *path;

    uri = req->uri;

    if (((strncmp(uri, CGI_BIN, 8)) == 0) && opt->cgi) {
	uri += 8;

	if ((path = malloc(PATH_MAX)) == NULL)
	    err(1, "malloc"); /* send 500 response */

	path = strdup(info->cgi_dir);

	(void)strncat(path, uri, PATH_MAX - strlen(path) - 1);
	printf("cgi path : %s\n", path);
	/* TODO send response headers */
	cgi(client, path);
	return;
    }

    /* TODO Check that uri is a regular file or a directory
     * that contains 'index.html'.  Might be efficient to call
     * fts_open(3) here and then reuse the result in `listing' */

    /* Need to list out the directory at this point */
    if ((path = malloc(PATH_MAX)) == NULL)
	err(1, "malloc");
    path = strdup(info->dir);
    (void)strncat(path, uri, PATH_MAX - strlen(path) - 1);

    /* call realpath(3)? */

    listing(client, path, req->time);
    free(path);
}
