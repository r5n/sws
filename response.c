#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define ARRAY_LEN(arr) ((sizeof arr)/(sizeof arr[0]))
#define DATE_FMT     "%a, %d %b %Y %H:%M:%S GMT"
#define SERVER_INFO  "sws/1.0"

typedef struct {
    int code;
    const char *text;
} response_code;

const response_code responses[] = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 500, "Internal Server Error" }
};

void
internal_error(int fd)
{
    respond(fd, &(response){
        .last_modified = NULL,
        .content = NULL,
        .code = 500
    });
}

void
respond(int fd, response *resp)
{
    time_t now;
    char tmbuf[BUFSIZ], lmbuf[BUFSIZ];
    const char *reason = "Unknown";

    if (time(&now) == -1)
        err(1, "time");

    if (strftime(tmbuf, BUFSIZ, DATE_FMT, gmtime(&now)) == 0)
        err(1, "strftime");

    if (resp->last_modified != NULL)
        if (strftime(lmbuf, BUFSIZ, DATE_FMT, resp->last_modified) == 0)
            err(1, "strftime");

    for (unsigned i = 0; i < ARRAY_LEN(responses); ++i)
        if (resp->code == responses[i].code)
            reason = responses[i].text;

    dprintf(fd, "HTTP/1.0 %d %s\r\n", resp->code, reason);
    dprintf(fd, "Date: %s\r\n", tmbuf);
    dprintf(fd, "Server: %s\r\n", SERVER_INFO);
    if (resp->last_modified != NULL)
        dprintf(fd, "Last-Modified: %s\r\n", lmbuf);
    if (resp->content && resp->content_type)
        dprintf(fd, "Content-Type: %s\r\n", resp->content_type);
    dprintf(fd, "Content-Length: %zu\r\n", resp->content ? strlen(resp->content) : 0);

    dprintf(fd, "\r\n");

    if (resp->content)
        dprintf(fd, "%s", resp->content);
}

void
handle_request(int client, struct options *opt,
	       struct server_info *info, struct http_request *req, char *cwd)
{
    response resp;
    char *full, *real, *path, *uri;
    time_t now;

    uri = req->uri;

    if (time(&now) == -1)
        err(1, "time");

    if (((strncmp(uri, "/cgi-bin", 8)) == 0) && opt->cgi) {
        uri += 8;

        printf("cgi part ran\n");

        if ((path = malloc(PATH_MAX)) == NULL) {
            internal_error(client);
            err(1, "malloc");
        }

        path = strdup(info->cgi_dir);
        (void)strncat(path, uri, PATH_MAX - strlen(path) - 1);

        cgi(path, &resp);
    } else {
        full = malloc(strlen(cwd) + 1 + strlen(req->uri));
        if (!full)
            err(1, "malloc");

        sprintf(full, "%s/%s", cwd, req->uri);
        real = realpath(full, NULL);
        if (!real) {
            if (errno == ENOENT) {
                // security vulnerability - leaks the existence of files
                resp = (response){
                    .last_modified = NULL,
                    .content = NULL,
                    .code = 404
                };
            } else {
                fprintf(stderr, "realpath(%s): %s\n", full, strerror(errno));
                resp = (response){
                    .last_modified = NULL,
                    .content = NULL,
                    .code = 500
                };
            }
        } else {
            // check that we don't serve files outside the public dir
            char *parent = real;
            bool forbidden = false;

            while (true) {
                if (strcmp(cwd, parent) == 0)
                    break; // all good
                else if (strstr(parent, cwd) == parent)
                    parent = dirname(parent);
                else {
                    forbidden = true;
                    break;
                }
            }

            if (forbidden) {
                //const char *response = "HTTP/1.0 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
                //write(fd, response, strlen(response));
            } else {
                //const char *response = "HTTP/1.0 200 Ok\r\nContent-Length: 0\r\n\r\n";
                //write(fd, response, strlen(response));
            }

            free(real);
        }

        respond(client, &resp);
        return;
    }

    /* TODO Check that uri is a regular file or a directory
     * that contains 'index.html'.  Might be efficient to call
     * fts_open(3) here and then reuse the result in `listing' */

    /* Need to list out the directory at this point */
    if ((path = malloc(PATH_MAX)) == NULL) {
        internal_error(client);
        err(1, "malloc");
    }
    path = strdup(info->dir);
    (void)strncat(path, uri, PATH_MAX - strlen(path) - 1);

    printf("listing path: %s\n", path);

    listing(client, path, req->time, &resp);
    respond(client, &resp);

    free(path);
}
