#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define BAD_REQ      "Bad Request"
#define INT_SRV_ERR  "Internal Server Error"
#define INT_SRV_ERR_LEN 21
#define BAD_REQ_LEN  11
#define CRLF         "\r\n"
#define DATE_FMT     "%a, %d %b %Y %H:%M:%S GMT"
#define HTTP_TYPE    "HTTP/1.0"
#define SERVER_INFO  "sws/1.0"
#define TEXT         "something something dark side"
#define PLAIN_STR    "text/plain"
#define PLAIN_LEN    10

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
write_server_error(int fd)
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
    resp->status = 500;
    resp->content_length = strlen(INT_SRV_ERR);

    if ((resp->content_type = malloc(PLAIN_LEN+1)) == NULL)
	err(1, "malloc");
    
    (void)strncpy(resp->content_type, "text/plain", PLAIN_LEN);

    resp->content_type[PLAIN_LEN+1] = '\0';

    if ((resp->reason = malloc(INT_SRV_ERR_LEN+1)) == NULL)
	err(1, "malloc");

    (void)strncpy(resp->reason, INT_SRV_ERR, INT_SRV_ERR_LEN);
    resp->reason[INT_SRV_ERR_LEN+1] = '\0';

    resp_header(fd, resp);
    dprintf(fd, "%s" CRLF, INT_SRV_ERR);   /* For now */
    free_response(resp);
}
