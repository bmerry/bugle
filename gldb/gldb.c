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
#include "common/hashtable.h"
/* Uncomment these lines to test the legacy I/O support
#undef HAVE_READLINE_READLINE_H
#define HAVE_READLINE_READLINE_H 0
*/

#define RESTART(expr) \
    do \
    { \
        while ((expr) == -1) \
        { \
            if (errno != EINTR) \
            { \
                perror(#expr " failed"); \
                exit(1); \
            } \
        } \
    } while (0)

static int lib_in, lib_out;
static sigset_t blocked, unblocked;
static sigjmp_buf chld_env;
/* Started is set to true only after receiving RESP_RUNNING. When
 * we are in the state (running && !started), non-break responses are
 * handled but do not cause a new line to be read.
 */
static bool running = false, started = false;
static pid_t child_pid;
static const char *chain;
static const char *prog;

/* The table of legal commands. The keys are the (possibly abbreviated)
 * command names, and the values are the command_data structs defined later.
 * Ambiguous commands have a NULL handler. The handler takes the canonical
 * name of the command, the raw line data, and a NULL-terminated array of
 * fields.
 */
static hash_table command_table;

static bool break_on_error = true;
static hash_table break_on;

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
    if (r == -1)
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
static pid_t execute(void)
{
    pid_t pid;
    /* in/out refers to our view, not child view */
    int in_pipe[2], out_pipe[2];
    char *env;

    check(pipe(in_pipe), "pipe");
    check(pipe(out_pipe), "pipe");
    switch ((pid = fork()))
    {
    case -1:
        perror("fork failed");
        exit(1);
    case 0: /* Child */
        xsetenv("BUGLE_CHAIN", chain);
        xsetenv("LD_PRELOAD", LIBDIR "/libbugle.so");
        xsetenv("BUGLE_DEBUGGER", "1");
        xasprintf(&env, "%d", in_pipe[1]);
        xsetenv("BUGLE_DEBUGGER_FD_OUT", env);
        free(env);
        xasprintf(&env, "%d", out_pipe[0]);
        xsetenv("BUGLE_DEBUGGER_FD_IN", env);
        free(env);

        close(in_pipe[0]);
        close(out_pipe[1]);
        execlp(prog, prog, NULL);
        execl(prog, prog, NULL);
        perror("failed to execute program");
        exit(1);
    default: /* Parent */
        lib_in = in_pipe[0];
        lib_out = out_pipe[1];
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

static bool command_cont(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    send_code(lib_out, REQ_CONT);
    return true;
}

static bool command_kill(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    send_code(lib_out, REQ_QUIT);
    return true;
}

static bool command_quit(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    if (running) send_code(lib_out, REQ_QUIT);
    exit(0);
}

static bool command_break_unbreak(const char *cmd,
                                  const char *line,
                                  const char * const *tokens)
{
    const char *func;
    uint32_t req_val;

    func = tokens[1];
    if (!func)
    {
        fputs("Specify a function or \"error\".\n", stderr);
        return false;
    }

    req_val = strcmp(cmd, "break") == 0;
    if (strcmp(func, "error") == 0)
    {
        break_on_error = req_val;
        if (running)
        {
            send_code(lib_out, REQ_BREAK_ERROR);
            send_code(lib_out, req_val);
        }
    }
    else
    {
        hash_set(&break_on, func, req_val ? "1" : "0");
        if (running)
        {
            send_code(lib_out, REQ_BREAK);
            send_string(lib_out, func);
            send_code(lib_out, req_val);
        }
    }
    return running;
}

static bool command_run(const char *cmd,
                        const char *line,
                        const char * const *tokens)
{
    const hash_entry *h;

    if (running)
    {
        fputs("Already running\n", stderr);
        return false;
    }
    else
    {
        child_pid = execute();
        /* Send breakpoints */
        send_code(lib_out, REQ_BREAK_ERROR);
        send_code(lib_out, break_on_error ? 1 : 0);
        for (h = hash_begin(&break_on); h; h = hash_next(&break_on, h))
        {
            send_code(lib_out, REQ_BREAK);
            send_string(lib_out, h->key);
            send_code(lib_out, *(const char *) h->value - '0');
        }
        send_code(lib_out, REQ_RUN);
        running = true;
        started = false;
        return true;
    }
}

static bool command_backtrace(const char *cmd,
                              const char *line,
                              const char * const *tokens)
{
    int in_pipe[2], out_pipe[2];
    pid_t pid;
    char *pid_str;
    char *ln;
    FILE *in, *out;
    int status;

    check(pipe(in_pipe), "pipe");
    check(pipe(out_pipe), "pipe");
    /* FIXME: avoid triggering SIGCHLD when gdb dies */
    switch (pid = fork())
    {
    case -1:
        perror("fork");
        exit(1);
    case 0: /* child */
        xasprintf(&pid_str, "%ld", (long) child_pid);
        /* FIXME: if in_pipe[1] or out_pipe[0] is already 0 or 1 */
        check(dup2(in_pipe[1], 1), "dup2");
        check(dup2(out_pipe[0], 0), "dup2");
        if (in_pipe[0] != 0 && in_pipe[0] != 1) check(close(in_pipe[0]), "close");
        if (in_pipe[1] != 0 && in_pipe[1] != 1) check(close(in_pipe[1]), "close");
        if (out_pipe[0] != 0 && out_pipe[0] != 1) check(close(out_pipe[0]), "close");
        if (out_pipe[0] != 1 && out_pipe[1] != 1) check(close(out_pipe[1]), "close");
        execlp("gdb", "gdb", "-batch", "-nx", "-command", "/dev/stdin", prog, pid_str, NULL);
        perror("could not invoke gdb");
        free(pid_str);
        exit(1);
    default: /* parent */
        check(close(in_pipe[1]), "close");
        check(close(out_pipe[0]), "close");
        check((in = fdopen(in_pipe[0], "r")) ? 0 : -1, "fdopen");
        check((out = fdopen(out_pipe[1], "w")) ? 0 : -1, "fdopen");
        fprintf(out, "backtrace\nquit\n");
        fflush(out);
        while ((ln = xafgets(in)) != NULL)
        {
            /* gdb backtrace lines start with # */
            if (*ln == '#') fputs(ln, stdout);
            free(ln);
        }
        RESTART(waitpid(pid, &status, 0));
    }
    return false;
}

typedef struct
{
    bool root;             /* i.e. not an abbreviation */
    bool running;          /* only makes sense when program running */
    /* The canonical name of the command. The memory belongs to the root */
    char *root_name;
    bool (*handler)(const char *, const char *, const char * const *);
} command_data;

static void register_command(const char *name, bool running,
                             bool (*handler)(const char *,
                                             const char *,
                                             const char * const *))
{
    command_data *data;
    char *root_name, *end;

    root_name = xstrdup(name);
    data = (command_data *) hash_get(&command_table, name);
    if (data)
        assert(!data->root); /* can't redefine commands */
    else
        data = (command_data *) xmalloc(sizeof(command_data));
    data->root = true;
    data->running = running;
    data->root_name = root_name;
    data->handler = handler;
    hash_set(&command_table, name, data);

    /* Dirty hack announcement: we progressively shorten the root_name
     * string for indexing the table, then restore it later so that
     * the pointers we are copying become correct again.
     */
    end = root_name + strlen(root_name);
    *--end = '\0';
    while (end > root_name)
    {
        data = (command_data *) hash_get(&command_table, root_name);
        if (!data)
        {
            data = (command_data *) xmalloc(sizeof(command_data *));
            data->root = false;
            data->running = running;
            data->root_name = root_name;
            data->handler = handler;
            hash_set(&command_table, root_name, data);
        }
        else if (!data->root)
            data->handler = NULL; /* ambiguate it */
        *--end = '\0';
    }
    strcpy(root_name, name);
}

static void handle_commands(void)
{
    bool done;
    char *line, *cur, *base;
    char **tokens;
    size_t num_tokens, i;
    const command_data *data;

    do
    {
        done = false;
#if HAVE_READLINE_READLINE_H
        line = readline("(gldb) ");
        if (line && *line) add_history(line);
#else
        fputs("(gldb) ", stdout);
        fflush(stdout);
        if ((line = xafgets(stdin)) != NULL)
            chop(line);
#endif
        if (!line)
        {
            if (running) send_code(lib_out, REQ_QUIT);
            exit(0); /* FIXME: clean shutdown? */
        }
        else if (*line)
        {
            /* Tokenise */
            num_tokens = 0;
            for (cur = line; *cur; cur++)
                if (!isspace(*cur)
                    && (cur == line || isspace(cur[-1])))
                    num_tokens++;
            if (num_tokens)
            {
                tokens = xmalloc((num_tokens + 1) * sizeof(char *));
                tokens[num_tokens] = NULL;
                i = 0;
                for (cur = line; *cur; cur++)
                {
                    if (!isspace(*cur))
                    {
                        base = cur;
                        while (*cur && !isspace(*cur)) cur++;
                        tokens[i] = xmalloc(cur - base + 1);
                        memcpy(tokens[i], base, cur - base);
                        tokens[i][cur - base] = '\0';
                        i++;
                        cur--; /* balanced by cur++ above */
                    }
                }
                assert(i == num_tokens);
                /* Find the command */
                data = (const command_data *) hash_get(&command_table, tokens[0]);
                if (!data)
                    fprintf(stderr, "Unknown command `%s'.\n", tokens[0]);
                else if (!data->handler)
                    fprintf(stderr, "Ambiguous command `%s'.\n", tokens[1]);
                else if (data->running && !running)
                    fprintf(stderr, "Program is not running.\n");
                else
                    done = data->handler(data->root_name, line, (const char * const *) tokens);
                for (i = 0; i < num_tokens; i++)
                    free(tokens[i]);
                free(tokens);
            }
        }
        if (line) free(line);
    } while (!done);
}

/* Deals with response code resp. Returns true if we are done processing
 * responses, or false if we are expecting more.
 */
static bool handle_responses(uint32_t resp)
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
        printf("Break on %s.\n", resp_str);
        free(resp_str);
        break;
    case RESP_BREAK_ERROR:
        recv_string(lib_in, &resp_str);
        recv_string(lib_in, &resp_str2);
        printf("Error %s in %s.\n", resp_str2, resp_str);
        free(resp_str);
        free(resp_str2);
        break;
    case RESP_ERROR:
        recv_code(lib_in, &resp_val);
        recv_string(lib_in, &resp_str);
        printf("%s\n", resp_str);
        free(resp_str);
        break;
    case RESP_RUNNING:
        printf("Running.\n");
        started = true;
        return false;
    }
    return started;
}

static void main_loop(void)
{
    int status;
    uint32_t resp;

    setup_sigchld();
    while (true)
    {
        if (sigsetjmp(chld_env, 1))
        {
            /* If we get here, then the child died. */
            RESTART(waitpid(child_pid, &status, 0));
            printf("Program exited\n");
            assert(running == true);
            running = false;
            started = false;
            continue;
        }

        if (!running || started)
            handle_commands();

        /* Wait for either input on the pipe, or for the child to die.
         * In the latter case we hit the sigsetjmp above.
         */
        do
        {
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
        }
        while (!handle_responses(resp));
    }
}

static void shutdown(void)
{
    const hash_entry *h;
    command_data *data;

    for (h = hash_begin(&command_table); h; h = hash_next(&command_table, h))
    {
        data = (command_data *) h->value;
        if (data->root) free(data->root_name);
        free(data);
    }
    hash_clear(&command_table, false);
    hash_clear(&break_on, true);
}

static void initialise(void)
{
    initialise_hashing();
    hash_init(&break_on);
    hash_init(&command_table);
    register_command("quit", false, command_quit);
    register_command("continue", true, command_cont);
    register_command("break", false, command_break_unbreak);
    register_command("unbreak", false, command_break_unbreak);
    register_command("kill", true, command_kill);
    register_command("run", false, command_run);
    register_command("backtrace", true, command_backtrace);
    register_command("bt", true, command_backtrace);
    atexit(shutdown);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fputs("Usage: gldb <chain> <program>\n", stderr);
        return 1;
    }
    chain = argv[1];
    prog = argv[2];
    initialise();
    main_loop();
    return 0;
}
