#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define HUMANIZE_LEN  5
#define STRTIME_FMT   "%Y-%m-%d %H:%M "
#define STRTIME_LEN   17

int
num_len(off_t n)
{
    if (n <= 0) return 1;
    return (int)floor(log10((float)n)) + 1;
}

void
html_header(char **buf, size_t *bufsz, size_t *buflen, char *path)
{
    int n;
    char tmp[BUFSIZ];

    n = snprintf(tmp, BUFSIZ,
         "<!DOCTYPE HTML>\n"
         "<html>\n<head><title>Index of %s</title></head>\n"
         "<body>\n<h1>Index of %s</h1>\n<table>\n"
         "<tr><td><b>Name</b></td><td><b>Last Modified</b></td>"
         "<td><b>Size</b></td>",
         path, path);
    if (n < 0)
        err(1, "snprintf");

    while (n >= (int)(*bufsz - *buflen)) {
        if ((*buf = realloc(*buf, *bufsz + n + 1)) == NULL)
            err(1, "realloc");
        *bufsz += n + 1;
    }
    (void)strncpy(*buf+(*buflen), tmp, n + 1);
    *buflen += n;
}

void
html_footer(char **buf, size_t *bufsz, size_t *buflen)
{
    int n;
    char tmp[BUFSIZ];

    n = snprintf(tmp, BUFSIZ, "\n</table>\n</body>\n</html>");
    if (n < 0)
        err(1, "snprintf");

    if (n >= (int)(*bufsz - *buflen)) {
        if ((*buf = realloc(*buf, *bufsz + n + 1)) == NULL)
            err(1, "realloc");
        *bufsz += n + 1;
    }

    (void)strncpy(*buf + (*buflen), tmp, n + 1);
    *buflen += n;
}

/* change for NetBSD --> int sz to char *sz */
void
write_entry(char **buf, size_t *bufsz, size_t *buflen,
        char *name, char *tm, int sz, bool isdir)
{
    int n;
    char tmp[BUFSIZ];

    n = snprintf(tmp, BUFSIZ,
                 "<tr>\n"
                 "<td><a href=\"%s%s\">%s%s</a></td><td>%s</td><td>%d</td>\n"
                 "</tr>\n",
                 name, isdir ? "/" : "", name, isdir ? "/" : "", tm, sz);
    if (n < 0)
        err(1, "snprintf");

    if (n >= (int)(*bufsz - *buflen)) {
        if ((*buf = realloc(*buf, *bufsz + n + 1)) == NULL)
            err(1, "realloc");
        *bufsz += n + 1;
    }
    (void)strncpy(*buf+(*buflen), tmp, n + 1);
    *buflen += n;
}

void
listing(int fd, char *target, struct http_request *req, response *resp)
{
    DIR *dp;
    struct dirent *dirp;
    time_t tmod;
    struct tm *tp, *mod;
    char buf[PATH_MAX + 1], fpath[PATH_MAX + 1], tbuf[STRTIME_LEN];
    struct stat st;
    size_t size, len;
    char *path;

    size = BUFSIZ;
    len = 0;
    path = target;

    mod = req->time;
    if (req->if_modified)
        tmod = mktime(mod);

    if ((dp = opendir(path)) == NULL)
        err(1, "opendir"); // TODO send response to client instead

    if ((resp->content = calloc(size, 1)) == NULL) {
        internal_error(fd, req);
        err(1, "calloc");
    }

    html_header(&resp->content, &size, &len, req->uri);

    for (;;) {
        if ((dirp = readdir(dp)) == NULL)
            break;

        if (strncmp(dirp->d_name, ".", 1) == 0)
            continue;

        (void)snprintf(buf, PATH_MAX, "%s/%s", path, dirp->d_name);
        (void)realpath(buf, fpath);

        if ((stat(fpath, &st)) == -1)
            err(1, "stat");

        if (req->if_modified == 1) {
            if (difftime(st.st_mtime, tmod) < 0) {
                continue;
            }
        }

        tp = gmtime(&st.st_mtime);
        if (tp == NULL) {
            internal_error(fd, req);
            err(1, "gmtime");
        }

        if (strftime(tbuf, sizeof(buf), "%Y-%m-%d %H:%M", tp) == 0) {
            internal_error(fd, req);
            err(1, "strftime");
        }

    #if 0
        if ((humanize_number(bufh, HUMANIZE_LEN, (int64_t)st.st_size,
                    suffix, HN_AUTOSCALE,
                    HN_DECIMAL | HN_NOSPACE | HN_B)) == -1) {
            internal_error(fd);
            err(1, "humanize_number");
        }
    #endif

        write_entry(&resp->content, &size, &len, dirp->d_name,
                tbuf, (int)st.st_size, S_ISDIR(st.st_mode));
    }
    
    html_footer(&resp->content, &size, &len);

    resp->content_type = "text/html";
    resp->code = 200;
    resp->last_modified = NULL;

    (void)closedir(dp);
}
