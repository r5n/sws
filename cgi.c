#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define ENV_SZ 50

char *
build_kv(const char *key, const char *value)
{
    char buf[BUFSIZ];
    char *kv;

    if ((strlen(key) + strlen(value)) > BUFSIZ)
	return NULL;

    (void)snprintf(buf, BUFSIZ, "%s=%s", key, value);
    kv = strdup(buf);
    return kv;
}

char **
make_cgienv(char *script, struct server_info *info, struct http_request *req)
{
    char **env;
    int n;

    n = 0;

    if ((env = calloc(ENV_SZ, sizeof(char *))) == NULL)
	return 0;
    
    env[n++] = build_kv("REQUEST_URI", req->uri);
    env[n++] = build_kv("PATH", info->cgi_dir);
    env[n++] = build_kv("SERVER_ADMIN", "sws@stevens.edu");
    env[n++] = build_kv("SCRIPT_NAME", script);
    env[n++] = build_kv("REQUEST_METHOD", req->type == GET ? "GET" : "HEAD");
    env[n++] = build_kv("SERVER_PROTOCOL", "HTTP/1.0");
    env[n++] = build_kv("SERVER_PORT", info->port);
    env[n++] = build_kv("SERVER_SOFTWARE", "sws/1.0");

    return env;
}

void
free_cgienv(char **env)
{
    int n;

    n = 0;
    while (env[n] != (char *)0) {
	free(env[n]);
	n++;
    }
    free(env);
}


void
do_cgi(char *path, struct server_info *info,
       struct http_request *req, response *resp)
{
    // char buf[BUFSIZ];
    pid_t pid;
    struct sigaction intsa, quitsa, sa;
    sigset_t nmask, omask;
    int p[2];
    int n;
    size_t bufsz, buflen;
    char **envp;

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

	resp->content_type = "text/html";
	resp->last_modified = NULL;
	resp->code = 200;

	sigaction(SIGINT, &intsa, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	
	return;
    } else {            /* child */
	close(p[0]);    /* close read end */
	if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO)
	    err(1, "dup2 to stdout");

	envp = make_cgienv(path, info, req);
	
	execle(path, "", NULL, envp);
	err(1, "execle");
    }
    free_cgienv(envp);
    sigaction(SIGINT, &intsa, NULL);
    sigaction(SIGQUIT, &quitsa, NULL);
    (void)sigprocmask(SIG_SETMASK, &omask, NULL);
}
