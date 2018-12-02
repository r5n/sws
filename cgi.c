#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int
cgi(int fd, char *path)
{
    char buf[BUFSIZ];
    pid_t pid;
    struct sigaction intsa, quitsa, sa;
    sigset_t nmask, omask;
    int p[2];
    int n;

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, &intsa) == -1)
	err(1, "unable to handle SIGINT");
    if (sigaction(SIGQUIT, &sa, &quitsa) == -1) {
	sigaction(SIGINT, &insta, NULL);
	err(1, "unable to handle SIGQUIT");
    }

    sigemptyset(&nmask);
    sigaddset(&nmask, SIGCHILD);
    if (sigprocmask(SIG_BLOCK, &nmask, &omask) == -1) {
	sigaction(SIGINT, &insta, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	err(1, "unable to handle SIGCHILD");
    }
    
    if (pipe(p) < 0) {
	sigaction(SIGINT, &insta, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	err(1, "pipe"); /* send 500 ? */
    }

    if ((pid = fork()) < 0) {
	sigaction(SIGINT, &insta, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	err(1, "fork");
    }
    else if (pid > 0) { /* parent */
	close(p[1]);    /* close write end */
	while ((n = read(p[0], buf, BUFSIZ)) > 0) {
	    if ((write(fd, buf, n)) != n)
		err(1, "write");
	}
	close(p[0]);
	if (waitpid(pid, NULL, 0) < 0)
	    err(1, "waitpid");

	sigaction(SIGINT, &insta, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	
	return 0;
    } else {            /* child */
	close(p[0]);    /* close read end */
	if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO)
	    err(1, "dup2 to stdout");

	execlp(path, "", (char *) 0);
	err(1, "execlp");
    }
    sigaction(SIGINT, &insta, NULL);
    sigaction(SIGQUIT, &quitsa, NULL);
    (void)sigprocmask(SIG_SETMASK, &omask, NULL);
}
