#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define CRLF         "\r\n"
#define DATE_FMT     "%a, %d %b %Y %H:%M:%S GMT"
#define HTTP_TYPE    "HTTP/1.0"
#define SERVER_INFO  "sws/1.0"
#define TEXT         "something something dark side"

struct http_response {
    struct tm *last_modified;
    char *content_type;
    char *reason;
    size_t content_length;
    int status;
};

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

struct http_response *
sample_response(void)
{
    time_t now;
    struct tm *tm_p;
    
    time(&now);
    tm_p = gmtime(&now);
	
    struct http_response *resp;
    if ((resp = malloc(sizeof(struct http_response))) == NULL)
	err(1, "malloc");

    resp->status = 200;
    resp->content_length = strlen(TEXT);
    resp->last_modified = tm_p;

    if ((resp->content_type = malloc(sizeof(char *) * BUFSIZ)) == NULL)
	err(1, "malloc");
    (void)strncpy(resp->content_type, "text/plain", BUFSIZ - 1);
    resp->content_type[BUFSIZ] = '\0';

    if ((resp->reason = malloc(sizeof(char *) * BUFSIZ)) == NULL)
	err(1, "malloc");
    (void)strncpy(resp->reason, "OK", BUFSIZ - 1);
    resp->reason[BUFSIZ] = '\0';

    return resp;
}

void
write_sample_response(int fd)
{
    struct http_response *resp;
    resp = sample_response();
    resp_header(fd, resp);
    dprintf(fd, "%s" CRLF, TEXT);
}
