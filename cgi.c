#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

void
cgi(char *path, struct http_response *resp)
{
    // char buf[BUFSIZ];
    pid_t pid;
    struct sigaction intsa, quitsa, sa;
    sigset_t nmask, omask;
    int p[2];
    int n;
    size_t bufsz, buflen;

    bufsz = BUFSIZ;
    buflen = 0;

    if ((resp->content = calloc(1, bufsz)) == NULL)
	err(1, "calloc");

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, &intsa) == -1)
	err(1, "unable to handle SIGINT");
    if (sigaction(SIGQUIT, &sa, &quitsa) == -1) {
	sigaction(SIGINT, &intsa, NULL);
	err(1, "unable to handle SIGQUIT");
    }

    sigemptyset(&nmask);
    sigaddset(&nmask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &nmask, &omask) == -1) {
	sigaction(SIGINT, &intsa, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	err(1, "unable to handle SIGCHLD");
    }
    
    if (pipe(p) < 0) {
	sigaction(SIGINT, &intsa, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	err(1, "pipe"); /* send 500 ? */
    }

    if ((pid = fork()) < 0) {
	sigaction(SIGINT, &intsa, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	err(1, "fork");
    }
    else if (pid > 0) { /* parent */
	close(p[1]);    /* close write end */
	while (1) {
	    if ((n = read(p[0], resp->content + buflen, bufsz - buflen)) == 0)
		break;
	    if (n < 0) {
		sigaction(SIGINT, &intsa, NULL);
		sigaction(SIGQUIT, &quitsa, NULL);
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		err(1, "read");
	    }
	    if ((size_t) n == bufsz - buflen) {
		bufsz *= 2;
		if ((resp->content = realloc(resp->content, bufsz)) == NULL) {
		    sigaction(SIGINT, &intsa, NULL);
		    sigaction(SIGQUIT, &quitsa, NULL);
		    (void)sigprocmask(SIG_SETMASK, &omask, NULL);
		    err(1, "realloc");
		}
	    }
	    buflen += n;
	}
	close(p[0]);
	if (waitpid(pid, NULL, 0) < 0)
	    err(1, "waitpid");

	resp->content_length = buflen;
	resp->content_type = strdup("text/html");
	resp->last_modified = NULL;
	resp->reason = strdup("OK");
	resp->status = 200;

	sigaction(SIGINT, &intsa, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	
	return;
    } else {            /* child */
	close(p[0]);    /* close read end */
	if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO)
	    err(1, "dup2 to stdout");

	execlp(path, "", (char *) 0);
	err(1, "execlp");
    }
    sigaction(SIGINT, &intsa, NULL);
    sigaction(SIGQUIT, &quitsa, NULL);
    (void)sigprocmask(SIG_SETMASK, &omask, NULL);
}
