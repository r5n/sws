#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

#define CGI_BIN  "/cgi-bin"

void
handle_cgi(int fd, struct http_request *req)
{
    char *path;

    path = req->uri;

    if ((strncmp(path, CGI_BIN, 8)) != 0)
	return;

    path += 8;
    cgi(fd, path);
}

void
cgi(int fd, char *path)
{
    int n;
    int p[2];
    pid_t pid;
    char buf[BUFSIZ];

    if (pipe(p) < 0)
	err(1, "pipe"); /* send 500 ? */

    if ((pid = fork()) < 0)
	err(1, "fork");
    else if (pid > 0) { /* parent */
	close(p[1]);    /* close write end */
	while ((n = read(p[0], buf, BUFSIZ)) > 0) {
	    if ((write(fd, buf, n)) != n)
		err(1, "write");
	}
	close(p[0]);
	if (waitpid(pid, NULL, 0) < 0)
	    err(1, "waitpid");
	exit(0);
    } else {            /* child */
	close(p[0]);    /* close read end */
	if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO)
	    err(1, "dup2 to stdout");
	execlp(path, "", (char *) 0);
	err(1, "execlp");
    }
}
