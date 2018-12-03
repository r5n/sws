#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FTS_OPTIONS   FTS_PHYSICAL | FTS_NOCHDIR | FTS_SEEDOT
#define HUMANIZE_LEN  5
#define STRTIME_FMT   "%Y-%m-%d %H:%M "
#define STRTIME_LEN   17

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

/* Write out contents of directory to fd; ignore files older than `mod' */
void
listing(int fd, char *path, struct tm *mod)
{
    FTS *fp;
    FTSENT *entp, *chlp, *tmp;
    struct stat *st;
    time_t tmod;
    struct tm *tp;
    char *dir[2];
    // char bufh[HUMANIZE_LEN];     /* NetBSD */
    char buft[STRTIME_LEN];
    int maxnl, maxsz;
    // char *suffix;                /* NetBSD */

    dir[0] = path;
    dir[1] = NULL;
    maxnl = maxsz = 0;
    
    if (mod != NULL)
	tmod = mktime(mod);

    if ((fp = fts_open(dir, FTS_OPTIONS, cmpfn)) == NULL)
	err(1, "fts_open");

    if ((entp = fts_read(fp)) == NULL)
	err(1, "fts_read");

    if ((chlp = fts_children(fp, 0)) == NULL)
	err(1, "fts_children");

    tmp = chlp;
    for ( ; tmp != NULL ; tmp = tmp->fts_link) { /* find limits */
	st = tmp->fts_statp;

	if ((tmp->fts_name[0] == '.') ||
	    (mod != NULL && (difftime(st->st_mtime, tmod) > 0)))
	    continue;

	if ((int)tmp->fts_namelen > maxnl)
	    maxnl = (int)tmp->fts_namelen;
	
	if (num_len(st->st_size) > maxsz)
	    maxsz = num_len(st->st_size);
    }

    for ( ; chlp != NULL ; chlp = chlp->fts_link) {
	st = chlp->fts_statp;

	if ((chlp->fts_name[0] == '.') ||
	    (mod != NULL && (difftime(st->st_mtime, tmod) > 0)))
	    continue;

	dprintf(fd, "%-*s  ", maxnl, chlp->fts_name);

	tp = gmtime(&(st->st_mtime));
	if (tp == NULL)
	    err(1, "gmtime");
	
	if (strftime(buft, sizeof(buft), "%Y-%m-%d %H:%M", tp) == 0)
	    err(1, "strftime");

	dprintf(fd, "%s  ", buft);

#if 0	/* humanize_number(3) not in macOS, remove #if 0 for NetBSD */
	if ((humanize_number(bufh, HUMANIZE_LEN, (int64_t)st->st_size,
			     suffix, HN_AUTOSCALE,
			     HN_DECIMAL | HN_NOSPACE | HN_B)) == -1)
	    err(1, "humanize_number");
	dprintf(fd, "%*s", HUMANIZE_LEN, bufh);
#endif

	/* remove for NetBSD */
	dprintf(fd, "%*d\n", maxsz, (int)st->st_size);
    }
}
