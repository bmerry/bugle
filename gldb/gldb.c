/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _XOPEN_SOURCE
#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#if HAVE_READLINE_READLINE_H
# include <readline/readline.h>
# include <readline/history.h>
#endif
#include "common/bool.h"
#include "common/safemem.h"
#include "common/protocol.h"

static int lib_in, lib_out;
static sigset_t blocked, unblocked;
static sigjmp_buf chld_env;
static bool running = false;
static pid_t child_pid;

#if !HAVE_READLINE_READLINE_H
static void chop(char *s)
{
    size_t len = strlen(s);
    while (len && isspace(s[len - 1]))
        s[--len] = '\0';
}
#endif

static void check(int r, const char *str)
{
    if (r != 0)
    {
        perror(str);
        exit(1);
    }
}

static void xsetenv(const char *name, const char *value)
{
    char *env;

    /* FIXME: memory leak. Might not be avoidable though, and probably
     * not important since we do this right before exec. */
    env = xmalloc(strlen(name) + strlen(value) + 2);
    sprintf(env, "%s=%s", name, value);
    check(putenv(env), "putenv");
}

static void sigchld_handler(int sig)
{
    siglongjmp(chld_env, 1);
}

/* Spawns off the program, and returns the pid */
static pid_t execute(const char *chain,
                     const char *prog)
{
    pid_t pid;
    /* in/out refers to our view, not child view */
    int pipein[2], pipeout[2];
    char *env;

    check(pipe(pipein), "pipe");
    check(pipe(pipeout), "pipe");
    switch ((pid = fork()))
    {
    case -1:
        perror("fork failed");
        exit(1);
    case 0: /* Child */
        xsetenv("BUGLE_CHAIN", chain);
        xsetenv("LD_PRELOAD", LIBDIR "/libbugle.so");
        xsetenv("BUGLE_DEBUGGER", "1");
        xasprintf(&env, "%d", pipein[1]);
        xsetenv("BUGLE_DEBUGGER_FD_OUT", env);
        free(env);
        xasprintf(&env, "%d", pipeout[0]);
        xsetenv("BUGLE_DEBUGGER_FD_IN", env);
        free(env);

        fcntl(pipein[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipeout[1], F_SETFD, FD_CLOEXEC);
        execlp(prog, prog, NULL);
        perror("failed to execute program");
        exit(1);
    default: /* Parent */
        lib_in = pipein[0];
        lib_out = pipeout[1];
        return pid;
    }
}

static void setup_sigchld(void)
{
    struct sigaction act;

    /* In normal operation, we block SIGCHLD to avoid race condition.
     * We explicitly unblock it only when waiting for things to happen.
     */
    check(sigprocmask(SIG_BLOCK, NULL, &unblocked), "sigprocmask");
    check(sigprocmask(SIG_BLOCK, NULL, &blocked), "sigprocmask");
    sigaddset(&blocked, SIGCHLD);
    check(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");

    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigemptyset(&act.sa_mask);
    check(sigaction(SIGCHLD, &act, NULL), "sigaction");
}

static void handle_commands(const char *chain, const char *prog)
{
    bool more;
    char *line;      /* FIXME: handle longer, or use a lexical analyser */
    char func[128];  /* FIXME: buffer overflow */
    uint32_t req_val;

    do
    {
        more = !running;
#if HAVE_READLINE_READLINE_H
        line = readline("(gldb) ");
#else
        fputs("(gldb) ", stdout);
        fflush(stdout);
        line = xmalloc(128); /* FIXME: buffer overflow */
        if (fgets(line, sizeof(line), stdin) == NULL);
        {
            free(line);
            line = NULL;
        }
        else
            chop(line);
#endif
        if (!line)
        {
            if (running) send_code(lib_out, REQ_QUIT);
            exit(0); /* FIXME: clean shutdown? */
        }
        /* FIXME: use a lexical scanner */
        if (strcmp(line, "cont") == 0)
        {
            if (!running)
                fputs("Not running\n", stdout);
            else
                send_code(lib_out, REQ_CONT);
        }
        else if (strcmp(line, "kill") == 0)
        {
            if (!running)
                fputs("Not running\n", stdout);
            else
                send_code(lib_out, REQ_QUIT);
        }
        else if (strcmp(line, "quit") == 0)
        {
            if (running)
                send_code(lib_out, REQ_QUIT);
            exit(0);
        }
        else if (sscanf(line, "break %s", func) == 1
                 || sscanf(line, "unbreak %s", func) == 1)
        {
            req_val = sscanf(line, "break %s", func);
            if (strcmp(func, "error") == 0)
            {
                send_code(lib_out, REQ_BREAK_ERROR);
                send_code(lib_out, req_val);
            }
            else
            {
                send_code(lib_out, REQ_BREAK);
                send_string(lib_out, func);
                send_code(lib_out, req_val);
            }
        }
        else if (strcmp(line, "run") == 0)
        {
            if (running)
            {
                fputs("Already running\n", stderr);
                more = true;
            }
            else
            {
                child_pid = execute(chain, prog);
                send_code(lib_out, REQ_CONT); /* from initialisation */
                running = true;
                more = false;
            }
        }
        else
        {
            more = true;
            if (*line)
                printf("Unknown command `%s'\n", line);
        }
#if HAVE_READLINE_READLINE_H
        if (*line) add_history(line);
#endif
        free(line);
    } while (more);
}

static void handle_responses(uint32_t resp)
{
    uint32_t resp_val;
    char *resp_str, *resp_str2;

    switch (resp)
    {
    case RESP_ANS:
        recv_code(lib_in, &resp_val);
        /* Ignore, otherwise than to flush the pipe */
        break;
    case RESP_STOP:
        /* do nothing */
        break;
    case RESP_BREAK:
        recv_string(lib_in, &resp_str);
        printf("Break on %s\n", resp_str);
        free(resp_str);
        break;
    case RESP_BREAK_ERROR:
        recv_string(lib_in, &resp_str);
        recv_string(lib_in, &resp_str2);
        printf("Error %s in %s\n", resp_str2, resp_str);
        free(resp_str);
        free(resp_str2);
        break;
    case RESP_ERROR:
        recv_code(lib_in, &resp_val);
        recv_string(lib_in, &resp_str);
        printf("%s\n", resp_str);
        free(resp_str);
        break;
    }
}

static void main_loop(const char *chain, const char *prog)
{
    int status;
    uint32_t resp;

    setup_sigchld();
    while (true)
    {
        if (sigsetjmp(chld_env, 1))
        {
            /* If we get here, then the child died. */
            waitpid(child_pid, &status, 0);
            printf("Program exited\n");
            assert(running == true);
            running = false;
            continue;
        }

        handle_commands(chain, prog);

        /* Wait for either input on the pipe, or for the child to die.
         * In the latter case we hit the sigsetjmp above.
         */
        sigprocmask(SIG_SETMASK, &unblocked, NULL);
        if (!recv_code(lib_in, &resp))
        {
            /* Failure here means EOF, which hopefully means that the
             * program will now terminate. Cross thumbs...
             */
            sigprocmask(SIG_SETMASK, &blocked, NULL);
            sigsuspend(&unblocked);
            /* Hopefully we won't even reach here, but hit the sigsetjmp. */
            continue;
        }
        sigprocmask(SIG_SETMASK, &blocked, NULL);

        handle_responses(resp);
    }
}

int main(int argc, char **argv)
{
    main_loop(argv[1], argv[2]);
    return 0;
}
