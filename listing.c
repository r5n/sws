#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
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
    (void)strncpy(*buf+(*buflen), tmp, n - 1);
    *buflen += n - 1;
}

void
listing(int fd, char *path, struct tm *mod, struct http_response *resp)
{
    FTS *fp;
    FTSENT *entp, *chlp;
    struct stat *st;
    time_t tmod;
    struct tm *tp;
    char *dir[2];
    // char bufh[HUMANIZE_LEN];     /* NetBSD */
    char buft[STRTIME_LEN];
    size_t size, len;
    // char *suffix;                /* NetBSD */

    size = BUFSIZ;
    len = 0;

    if ((resp->content = calloc(size, 1)) == NULL) {
	internal_error(fd);
	err(1, "calloc");
    }
    
    dir[0] = path;
    dir[1] = NULL;

    if (mod != NULL)
	tmod = mktime(mod);

    if ((fp = fts_open(dir, FTS_OPTIONS, cmpfn)) == NULL) {
	err(1, "fts_open");
    }

    if ((entp = fts_read(fp)) == NULL)
	err(1, "fts_read");

    if ((chlp = fts_children(fp, 0)) == NULL)
	err(1, "fts_children");

    for (; chlp != NULL; chlp = chlp->fts_link) {
	st = chlp->fts_statp;

	if ((chlp->fts_name[0] == '.') ||
	    (mod != NULL && (difftime(st->st_mtime, tmod) > 0)))
	    continue;

	tp = gmtime(&(st->st_mtime));
	if (tp == NULL) {
	    internal_error(fd);
	    err(1, "gmtime");
	}

	if (strftime(buft, sizeof(buft), "%Y-%m-%d %H:%M", tp) == 0)
	    err(1, "strftime");

#if 0   /* NetBSD */
	if ((humanize_number(bufh, HUMANIZE_LEN, (int64_t)st->st_size,
			     suffix, HN_AUTOSCALE,
			     HN_DECIMAL | HN_NOSPACE | HN_B)) == -1) {
	    internal_error(fd);
	    err(1, "humanize_number");
	}
#endif

	write_entry(&resp->content, &size, &len,
		    chlp->fts_name, buft, (int)st->st_size);
    }

    resp->content_length = len;
    resp->content_type = strdup("text/html");
    resp->reason = strdup("OK");
    resp->status = 200;
    resp->last_modified = NULL;
}
