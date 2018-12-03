#include <sys/socket.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
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
        .content = "Internal server error",
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
    char *full, *real;
    struct stat st;
    bool cgi;
    int file;

    // for cgi we want "{info->cgi_dir}/${uri+8}"
    // otherwise, we want "{info->dir}/{uri}"

    cgi = strncmp(req->uri, "/cgi-bin", 8) == 0 && opt->cgi;

    if (cgi) {
        full = malloc(strlen(info->cgi_dir) + 1 + strlen(req->uri) - strlen("/cgi-bin"));
        if (!full)
            err(1, "malloc");
        sprintf(full, "%s/%s", info->cgi_dir, req->uri + 8);
    } else {
        full = malloc(strlen(cwd) + 1 + strlen(req->uri));
        if (!full)
            err(1, "malloc");
        sprintf(full, "%s/%s", cwd, req->uri);
    }

    real = realpath(full, NULL);
    if (!real) {
        if (errno == ENOENT) {
            // security vulnerability - leaks the existence of files
            respond(client, &(response){
                .last_modified = NULL,
                .content = "Not found",
                .code = 404
            });
            free(full);
            return;
        } else {
            fprintf(stderr, "realpath(%s): %s\n", full, strerror(errno));
            internal_error(client);
            free(full);
            return;
        }
    }

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
        resp = (response){
            .last_modified = NULL,
            .content = "Forbidden",
            .code = 403
        };
        goto end;
    }

    if (cgi) {
        do_cgi(real, &resp);
        goto end;
    }

    if (stat(real, &st) < 0) {
        resp = (response){
            .last_modified = NULL,
            .content = strerror(errno),
            .code = 403 // This is probably not always the correct response
        };
        goto end;
    }

    if (S_ISDIR(st.st_mode)) {
        listing(client, real, req, &resp);
    } else if ((file = open(real, O_RDONLY)) >= 0) {
        char buf[BUFSIZ], tmbuf[BUFSIZ], lmbuf[BUFSIZ];
        ssize_t rd;
        time_t now;
        char *dot, *mime;

        if (time(&now) == -1)
            err(1, "time");

        if (strftime(tmbuf, sizeof tmbuf, DATE_FMT, gmtime(&now)) == 0)
            err(1, "strftime");

        if (strftime(lmbuf, sizeof tmbuf, DATE_FMT, gmtime(&st.st_mtime)) == 0)
            err(1, "strftime");

        dprintf(client, "HTTP/1.0 200 OK\r\n");
        dprintf(client, "Date: %s\r\n", tmbuf);
        dprintf(client, "Server: %s\r\n", SERVER_INFO);
        dprintf(client, "Last-Modified: %s\r\n", lmbuf);

        dot = strrchr(real, '.');
        mime = "application/octet-stream";

        if (dot) {
            if (strcmp(dot, ".html") == 0)
                mime = "text/html";
            else if (strcmp(dot, ".txt") == 0)
                mime = "text/plain";
        }

        dprintf(client, "Content-Type: %s\r\n", mime);
        dprintf(client, "Content-Length: %lld\r\n", (long long)st.st_size);

        dprintf(client, "\r\n");

        while ((rd = read(file, buf, sizeof buf)) > 0) {
            if (write(client, buf, rd) < 0) {
                perror("write");
                break;
            }
        }

        if (rd < 0) {
            perror("read");
        }

        free(real);
        free(full);
        return;
    } else {
        perror(real);
        resp = (response){
            .last_modified = NULL,
            .content = strerror(errno),
            .code = 403
        };
    }

end:
    respond(client, &resp);
    free(real);
    free(full);
}
