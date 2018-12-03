#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

#define FTS_OPTIONS   FTS_PHYSICAL | FTS_NOCHDIR | FTS_SEEDOT
#define HUMANIZE_LEN  5
#define STRTIME_FMT   "%Y-%m-%d %H:%M "
#define STRTIME_LEN   17
#define ENT_FORMAT    "<tr>\n<td>%s</td><td>%s</td><td>%d</td>\n</tr>\n"

void write_entries(int, FTSENT *);

int
num_len(off_t n)
{
    if (n <= 0) return 1;
    return (int)floor(log10((float)n)) + 1;
}

int
cmpfn(const FTSENT **ent1, const FTSENT **ent2)
{
    short len;
    len = (*ent1)->fts_namelen;
    return strncmp((*ent1)->fts_name, (*ent2)->fts_name, len);
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
	*bufsz *= 2;
	if ((*buf = realloc(*buf, *bufsz)) == NULL)
	    err(1, "realloc");
    }
    (void)strncpy(*buf+(*buflen), tmp, n);
    *buflen += n;
}

void
html_footer(char **buf, size_t *bufsz, size_t *buflen)
{
    int n;
    char tmp[BUFSIZ];

    n = snprintf(tmp, BUFSIZ, "\n</table>\n</body>\n</html>\n");
    if (n < 0)
	err(1, "snprintf");

    while (n >= (int)(*bufsz - *buflen)) {
	*bufsz *= 2;
	if ((*buf = realloc(*buf, *bufsz)) == NULL)
	    err(1, "realloc");
    }
    (void)strncpy(*buf+(*buflen), tmp, n);
    *buflen += n;
}

/* change for NetBSD --> int sz to char *sz */
void
write_entry(char **buf, size_t *bufsz, size_t *buflen,
	    char *name, char *tm, int sz)
{
    int n;
    char tmp[BUFSIZ];

    n = snprintf(tmp, BUFSIZ, ENT_FORMAT, name, tm, sz);
    if (n < 0)
	err(1, "snprintf");
    
    while (n >= (int)(*bufsz - *buflen)) {
	*bufsz *= 2;
	if ((*buf = realloc(*buf, *bufsz)) == NULL)
	    err(1, "realloc");
    }
    (void)strncpy(*buf+(*buflen), tmp, n); // don't copy '\0'
    *buflen += n;
}

void
listing(int fd, char *target, struct tm *mod, struct http_response *resp)
{
    DIR *dp;
    struct dirent *dirp;
    time_t tmod;
    struct tm *tp;
    char buf[PATH_MAX + 1], fpath[PATH_MAX + 1], tbuf[STRTIME_LEN];
    struct stat st;
    size_t size, len;
    char *fav, *path;

    size = BUFSIZ;
    len = 0;
    path = target;

    if (mod != NULL)
	tmod = mktime(mod);

    fav = strstr(target, "favicon.ico");
    if (fav != NULL) { /* request from browser */
	*fav = '\0';
	path = strdup(target);
	printf("path: %s\n", path);
    }

    if ((dp = opendir(path)) == NULL)
	err(1, "opendir"); // TODO send response to client instead

    if ((resp->content = calloc(size, 1)) == NULL) {
	internal_error(fd);
	err(1, "calloc");
    }

    html_header(&resp->content, &size, &len, path);

    for (;;) {
	if ((dirp = readdir(dp)) == NULL)
	    break;

	if (strncmp(dirp->d_name, ".", 1) == 0)
	    continue;

	(void)snprintf(buf, PATH_MAX, "%s/%s", path, dirp->d_name);
	(void)realpath(buf, fpath);

	if ((stat(fpath, &st)) == -1)
	    err(1, "stat");

	if (mod != NULL) {
	    if (difftime(st.st_mtime, tmod) > 0) {
		printf("have to continue\n");
		continue;
	    }
	}

	tp = gmtime(&st.st_mtime);
	if (tp == NULL) {
	    internal_error(fd);
	    err(1, "gmtime");
	}

	if (strftime(tbuf, sizeof(buf), "%Y-%m-%d %H:%M", tp) == 0) {
	    internal_error(fd);
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
		    tbuf, (int)st.st_size);
    }
    
    html_footer(&resp->content, &size, &len);

    if (fav != NULL)
	free(path);  // free strdup call

    resp->content_length = len;
    resp->content_type = strdup("text/html");
    resp->reason = strdup("OK");
    resp->status = 200;
    resp->last_modified = NULL;

    (void)closedir(dp);
}
