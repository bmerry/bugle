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
#define _XOPEN_SOURCE 600 /* for unsetenv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#if HAVE_READLINE
# include <readline/readline.h>
# include <readline/history.h>
#endif
#include "src/names.h"
#include "src/glfuncs.h"
#include "common/bool.h"
#include "common/safemem.h"
#include "common/protocol.h"
#include "common/hashtable.h"
/* Uncomment these lines to test the legacy I/O support
#undef HAVE_READLINE
#define HAVE_READLINE
*/

#if HAVE_READLINE && !HAVE_RL_COMPLETION_MATCHES
# define rl_completion_matches completion_matches
#endif

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
static sigjmp_buf chld_env, int_env;
/* Started is set to true only after receiving RESP_RUNNING. When
 * we are in the state (running && !started), non-break responses are
 * handled but do not cause a new line to be read.
 */
static bool running = false, started = false;
static pid_t child_pid = -1;
static int child_status = 0; /* status returned from waitpid */
static char *chain = NULL;
static const char *prog = NULL;
static char **prog_argv = NULL;

/* The table of full command names. This is used for command completion
 * and display of help. There is also a hash table of command names
 * including prefixes. The handler takes the canonical name of the command,
 * the raw line data, and a NULL-terminated array of tokens. The
 * generator is used by readline to complete arguments; see the
 * readline documentation for more details.
 *
 * Note that setting "help" to NULL excludes the command from the
 * documentation. This is recommended for shorthand aliases.
 */
typedef bool (*command_handler)(const char *, const char *, const char * const *);
typedef struct
{
    const char *name;
    const char *help;
    bool running;
    command_handler handler;
    char * (*generator)(const char *text, int state);
} command_info;
command_info commands[];

/* The structure for indexing commands in a hash table. */
typedef struct
{
    bool root;             /* i.e. not an abbreviation */
    const command_info *command;
} command_data;
/* The table of legal commands. The keys are the (possibly abbreviated)
 * command names, and the values are the command_data structs defined above.
 * Ambiguous commands have a NULL command field.
 */
static bugle_hash_table command_table;

static bool break_on_error = true;
static bugle_hash_table break_on;
static char *screenshot_file = NULL;

#if !HAVE_READLINE
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

static void sigchld_handler(int sig)
{
    siglongjmp(chld_env, 1);
}

static void sigint_handler(int sig)
{
    siglongjmp(int_env, 1);
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
        /* We don't want the child to receive Ctrl-C, but we want to
         * allow it to write to the terminal. We move it into a background
         * process group, and it inherits SIG_IGN on SIGTTIN and SIGTTOU.
         *
         * However, we don't want the child to have SIGINT and SIGCHLD
         * blocked.
         */
        setpgid(0, 0);
        sigprocmask(SIG_SETMASK, &unblocked, NULL);

        if (chain)
            check(setenv("BUGLE_CHAIN", chain, 1), "setenv");
        else
            unsetenv("BUGLE_CHAIN");
        check(setenv("LD_PRELOAD", LIBDIR "/libbugle.so", 1), "setenv");
        check(setenv("BUGLE_DEBUGGER", "1", 1), "setenv");
        bugle_asprintf(&env, "%d", in_pipe[1]);
        check(setenv("BUGLE_DEBUGGER_FD_OUT", env, 1), "setenv");
        free(env);
        bugle_asprintf(&env, "%d", out_pipe[0]);
        check(setenv("BUGLE_DEBUGGER_FD_IN", env, 1), "setenv");
        free(env);

        close(in_pipe[0]);
        close(out_pipe[1]);
        execvp(prog, prog_argv);
        execv(prog, prog_argv);
        perror("failed to execute program");
        exit(1);
    default: /* Parent */
        lib_in = in_pipe[0];
        lib_out = out_pipe[1];
        close(in_pipe[1]);
        close(out_pipe[0]);
        return pid;
    }
}

static void setup_signals(void)
{
    struct sigaction act;

    /* In normal operation, we block SIGCHLD to avoid race conditions.
     * We explicitly unblock it only when waiting for things to happen.
     */
    check(sigprocmask(SIG_BLOCK, NULL, &unblocked), "sigprocmask");
    check(sigprocmask(SIG_BLOCK, NULL, &blocked), "sigprocmask");
    sigaddset(&blocked, SIGCHLD);
    sigaddset(&blocked, SIGINT);
    check(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");

    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGINT);
    check(sigaction(SIGCHLD, &act, NULL), "sigaction");

    act.sa_handler = sigint_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGCHLD);
    check(sigaction(SIGINT, &act, NULL), "sigaction");

    /* We do various things with process groups, and gdb does some more.
     * To prevent any embarrassing stoppages we simply allow everyone
     * to use the terminal. The only case where this might blow up is
     * if gldb is backgrounded by the shell.
     */
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    check(sigaction(SIGTTIN, &act, NULL), "sigaction");
    check(sigaction(SIGTTOU, &act, NULL), "sigaction");
}

static bool command_cont(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    gldb_send_code(lib_out, REQ_CONT);
    return true;
}

static bool command_kill(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    gldb_send_code(lib_out, REQ_QUIT);
    return true;
}

static bool command_quit(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    if (running) gldb_send_code(lib_out, REQ_QUIT);
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
        fputs("Specify a function or \"error\".\n", stdout);
        return false;
    }

    req_val = (cmd[0] == 'b');
    if (strcmp(func, "error") == 0)
    {
        break_on_error = req_val;
        if (running)
        {
            gldb_send_code(lib_out, REQ_BREAK_ERROR);
            gldb_send_code(lib_out, req_val);
        }
    }
    else
    {
        bugle_hash_set(&break_on, func, req_val ? "1" : "0");
        if (running)
        {
            gldb_send_code(lib_out, REQ_BREAK);
            gldb_send_string(lib_out, func);
            gldb_send_code(lib_out, req_val);
        }
    }
    return running;
}

static bool command_run(const char *cmd,
                        const char *line,
                        const char * const *tokens)
{
    const bugle_hash_entry *h;

    if (running)
    {
        fputs("Already running\n", stdout);
        return false;
    }
    else
    {
        child_pid = execute();
        /* Send breakpoints */
        gldb_send_code(lib_out, REQ_BREAK_ERROR);
        gldb_send_code(lib_out, break_on_error ? 1 : 0);
        for (h = bugle_hash_begin(&break_on); h; h = bugle_hash_next(&break_on, h))
        {
            gldb_send_code(lib_out, REQ_BREAK);
            gldb_send_string(lib_out, h->key);
            gldb_send_code(lib_out, *(const char *) h->value - '0');
        }
        gldb_send_code(lib_out, REQ_RUN);
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
    struct sigaction act, real_act;

    check(pipe(in_pipe), "pipe");
    check(pipe(out_pipe), "pipe");
    /* We don't want the SIGCHLD handler to perform the longjmp when gdb
     * exits. We disable the handler, and once we've re-enabled it we
     * use waitpid with WNOHANG to see if the real program died in the
     * meantime.
     */
    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    check(sigaction(SIGCHLD, &act, &real_act), "sigaction");
    check(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");

    switch (pid = fork())
    {
    case -1:
        perror("fork");
        exit(1);
    case 0: /* child */
        sigprocmask(SIG_SETMASK, &unblocked, NULL);
        bugle_asprintf(&pid_str, "%ld", (long) child_pid);
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
        while ((ln = bugle_afgets(in)) != NULL)
        {
            /* gdb backtrace lines start with # */
            if (*ln == '#') fputs(ln, stdout);
            free(ln);
        }
        RESTART(waitpid(pid, &status, 0));
    }
    check(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");
    check(sigaction(SIGCHLD, &real_act, NULL), "sigaction");
    /* Now check if the real child died in the meantime */
    pid = waitpid(child_pid, &child_status, WNOHANG);
    check(pid, "waitpid");
    /* A value of 2 indicates that we have done the waiting */
    if (pid == child_pid)
        siglongjmp(chld_env, 2);
    return false;
}

static bool command_chain(const char *cmd,
                          const char *line,
                          const char * const *tokens)
{
    if (!tokens[1])
    {
        fputs("Usage: chain <chain> OR chain none.\n", stdout);
        return false;
    }
    if (chain)
    {
        free(chain);
        chain = NULL;
    }
    if (strcmp(tokens[1], "none") == 0)
        fputs("Chain cleared.\n", stdout);
    else
    {
        chain = bugle_strdup(tokens[1]);
        printf("Chain set to %s.\n", chain);
    }
    if (running)
        fputs("The change will only take effect when the program is restarted.\n", stdout);
    return false;
}

static bool command_enable_disable(const char *cmd,
                                   const char *line,
                                   const char * const *tokens)
{
    uint32_t req;

    if (!tokens[1] || !*tokens[1])
    {
        fputs("Usage: enable|disable <filterset>\n", stdout);
        return false;
    }
    req = (strcmp(cmd, "enable") == 0) ? REQ_ENABLE_FILTERSET : REQ_DISABLE_FILTERSET;
    gldb_send_code(lib_out, req);
    gldb_send_string(lib_out, tokens[1]);
    return true;
}

static bool command_gdb(const char *cmd,
                        const char *line,
                        const char * const *tokens)
{
    pid_t pid, pgrp, tc_pgrp;
    char *pid_str;
    int status;
    struct sigaction act, real_act;
    bool fore;

    /* We don't want the SIGCHLD handler to perform the longjmp when gdb
     * exits. We disable the handler, and once we've re-enabled it we
     * use waitpid with WNOHANG to see if the real program died in the
     * meantime.
     */
    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    check(sigaction(SIGCHLD, &act, &real_act), "sigaction");
    check(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");

    /* By default, gdb will be in the same process group and thus we
     * will receive the same terminal control signals, like SIGINT.
     * gdb uses these signals but we don't want to know about them,
     * so we move gdb into its own foreground process group if possible.
     * Note that we don't check return values, since POSIX doesn't
     * require job control.
     */
    tc_pgrp = tcgetpgrp(1);
    pgrp = getpgrp();
    fore = pgrp == tc_pgrp && pgrp != -1;
    switch (pid = fork())
    {
    case -1:
        perror("fork");
        exit(1);
    case 0: /* child */
        setpgid(0, 0);
        if (fore) tcsetpgrp(1, getpgrp());
        sigprocmask(SIG_SETMASK, &unblocked, NULL);

        bugle_asprintf(&pid_str, "%ld", (long) child_pid);
        execlp("gdb", "gdb", prog, pid_str, NULL);
        perror("could not invoke gdb");
        free(pid_str);
        exit(1);
    default: /* parent */
        RESTART(waitpid(pid, &status, 0));
        /* Reclaim foreground */
        if (fore) tcsetpgrp(1, pgrp);
    }
    check(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");
    check(sigaction(SIGCHLD, &real_act, NULL), "sigaction");
    /* Now check if the real child died in the meantime */
    pid = waitpid(child_pid, &child_status, WNOHANG);
    check(pid, "waitpid");
    /* A return value of 2 indicates that we have done the waiting */
    if (pid == child_pid)
        siglongjmp(chld_env, 2);
    return false;
}

static bool command_state(const char *cmd,
                          const char *line,
                          const char * const *tokens)
{
    gldb_send_code(lib_out, REQ_STATE);
    gldb_send_string(lib_out, tokens[1] ? tokens[1] : "");
    return true;
}

static bool command_screenshot(const char *cmd,
                               const char *line,
                               const char * const *tokens)
{
    const char *fname;

    fname = tokens[1];
    if (!fname)
    {
        fputs("Specify a filename\n", stdout);
        return false;
    }
    gldb_send_code(lib_out, REQ_SCREENSHOT);
    if (screenshot_file) free(screenshot_file);
    screenshot_file = bugle_strdup(tokens[1]);
    return true;
}

static bool command_help(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    const command_info *c;

    for (c = commands; c->name; c++)
        if (c->help)
            printf("%-12s\t%s\n", c->name, c->help);
    return false;
}

static void register_command(const command_info *cmd)
{
    command_data *data;
    char *key, *end;

    key = bugle_strdup(cmd->name);
    data = (command_data *) bugle_hash_get(&command_table, key);
    if (data)
        assert(!data->root); /* can't redefine commands */
    else
        data = (command_data *) bugle_malloc(sizeof(command_data));
    data->root = true;
    data->command = cmd;
    bugle_hash_set(&command_table, key, data);

    /* Progressively shorten the key to create abbreviations. */
    end = key + strlen(key);
    *--end = '\0';
    while (end > key)
    {
        data = (command_data *) bugle_hash_get(&command_table, key);
        if (!data)
        {
            data = (command_data *) bugle_malloc(sizeof(command_data));
            data->root = false;
            data->command = cmd;
            bugle_hash_set(&command_table, key, data);
        }
        else if (!data->root)
            data->command = NULL; /* ambiguate it */
        *--end = '\0';
    }
    free(key);
}

static void handle_commands(void)
{
    bool done;
    char *line = NULL, *cur, *base;
    /* prev_line must be static, because we sometimes leave handle_commands
     * via a signal/longjmp pair.
     */
    static char *prev_line = NULL;
    char **tokens;
    size_t num_tokens, i;
    const command_data *data;
    struct sigaction act, old_act;
    sigset_t sigint_set;

    do
    {
        done = false;
#if HAVE_READLINE
        /* Readline replaces the SIGINT handler, but still chains to the
         * original. We set the handler to ignore for the readline()
         * call to avoid queuing up extra stop requests.
         */
        act.sa_handler = SIG_DFL;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);
        check(sigaction(SIGINT, &act, &old_act), "sigaction");
        sigemptyset(&sigint_set);
        sigaddset(&sigint_set, SIGINT);
        check(sigprocmask(SIG_UNBLOCK, &sigint_set, NULL), "sigprocmask");

        line = readline("(gldb) ");

        check(sigprocmask(SIG_BLOCK, &sigint_set, NULL), "sigprocmask");
        check(sigaction(SIGINT, &old_act, NULL), "sigaction");
        if (line && *line) add_history(line);
#else
        fputs("(gldb) ", stdout);
        fflush(stdout);
        if ((line = bugle_afgets(stdin)) != NULL)
            chop(line);
#endif
        if (!line)
        {
            if (running) gldb_send_code(lib_out, REQ_QUIT);
            if (prev_line) free(prev_line);
            exit(0);
        }
        else
        {
            if (!*line && prev_line != NULL)
            {
                free(line);
                line = bugle_strdup(prev_line);
            }
            if (*line)
            {
                if (prev_line) free(prev_line);
                prev_line = bugle_strdup(line);

                /* Tokenise */
                num_tokens = 0;
                for (cur = line; *cur; cur++)
                    if (!isspace(*cur)
                        && (cur == line || isspace(cur[-1])))
                        num_tokens++;
                if (num_tokens)
                {
                    tokens = bugle_malloc((num_tokens + 1) * sizeof(char *));
                    tokens[num_tokens] = NULL;
                    i = 0;
                    for (cur = line; *cur; cur++)
                    {
                        if (!isspace(*cur))
                        {
                            base = cur;
                            while (*cur && !isspace(*cur)) cur++;
                            tokens[i] = bugle_malloc(cur - base + 1);
                            memcpy(tokens[i], base, cur - base);
                            tokens[i][cur - base] = '\0';
                            i++;
                            cur--; /* balanced by cur++ above */
                        }
                    }
                    assert(i == num_tokens);
                    /* Find the command */
                    data = (const command_data *) bugle_hash_get(&command_table, tokens[0]);
                    if (!data)
                        printf("Unknown command `%s'.\n", tokens[0]);
                    else if (!data->command)
                        printf("Ambiguous command `%s'.\n", tokens[0]);
                    else if (data->command->running && !running)
                        printf("Program is not running.\n");
                    else
                        done = data->command->handler(data->command->name, line, (const char * const *) tokens);
                    for (i = 0; i < num_tokens; i++)
                        free(tokens[i]);
                    free(tokens);
                }
            }
            free(line);
        }
    } while (!done);
}

/* Deals with response code resp. Returns true if we are done processing
 * responses, or false if we are expecting more.
 */
static bool handle_responses(uint32_t resp)
{
    uint32_t resp_val, resp_len;
    char *resp_str, *resp_str2;
    FILE *f;

    switch (resp)
    {
    case RESP_ANS:
        gldb_recv_code(lib_in, &resp_val);
        /* Ignore, other than to flush the pipe */
        break;
    case RESP_STOP:
        /* do nothing */
        break;
    case RESP_BREAK:
        gldb_recv_string(lib_in, &resp_str);
        printf("Break on %s.\n", resp_str);
        free(resp_str);
        break;
    case RESP_BREAK_ERROR:
        gldb_recv_string(lib_in, &resp_str);
        gldb_recv_string(lib_in, &resp_str2);
        printf("Error %s in %s.\n", resp_str2, resp_str);
        free(resp_str);
        free(resp_str2);
        break;
    case RESP_ERROR:
        gldb_recv_code(lib_in, &resp_val);
        gldb_recv_string(lib_in, &resp_str);
        printf("%s\n", resp_str);
        free(resp_str);
        break;
    case RESP_RUNNING:
        printf("Running.\n");
        started = true;
        return false;
    case RESP_STATE:
        gldb_recv_string(lib_in, &resp_str);
        fputs(resp_str, stdout);
        free(resp_str);
        break;
    case RESP_SCREENSHOT:
        gldb_recv_binary_string(lib_in, &resp_len, &resp_str);
        if (!screenshot_file)
        {
            fputs("Unexpected screenshot data. Please contact the author.\n", stderr);
            break;
        }
        f = fopen(screenshot_file, "wb");
        if (!f)
        {
            fprintf(stderr, "Cannot open %s: %s\n", screenshot_file, strerror(errno));
            free(screenshot_file);
            break;
        }
        if (fwrite(resp_str, 1, resp_len, f) != resp_len || fclose(f) == EOF)
            fprintf(stderr, "Error writing %s: %s\n", screenshot_file, strerror(errno));
        free(resp_str);
        free(screenshot_file);
        screenshot_file = NULL;
        break;
    }
    return started;
}

static void main_loop(void)
{
    uint32_t resp;

    setup_signals();
    while (true)
    {
        /* POSIX makes some strange requirements on how sigsetjmp is
         * used. In particular the result is not guaranteed to be
         * assignable.
         */
        switch (sigsetjmp(chld_env, 1))
        {
        case 1: /* from the signal handler */
            RESTART(waitpid(child_pid, &child_status, 0));
            /* fall through */
        case 2: /* from normal code that detected the exit */
            if (WIFEXITED(child_status))
            {
                if (WEXITSTATUS(child_status) == 0)
                    printf("Program exited normally.\n");
                else
                    printf("Program exited with return code %d.\n", WEXITSTATUS(child_status));
            }
            else if (WIFSIGNALED(child_status))
                printf("Program was terminated with signal %d.\n", WTERMSIG(child_status));
            else
                printf("Program was terminated abnormally.\n");

            assert(running == true);
            running = false;
            started = false;
            break;
        case 0: /* normal return from sigsetjmp */

            /* Again, POSIX requirements prevent these two tests from being
             * merged.
             */
            if (sigsetjmp(int_env, 1))
            {
                if (started)
                {
                    /* Ctrl-C was pressed. Send an asynchonous stop request. */
                    gldb_send_code(lib_out, REQ_ASYNC);
                }
                else
                {
                    if (!running || started)
                        handle_commands();
                }
            }
            else /* default code */
            {
                if (!running || started)
                    handle_commands();
            }

            /* Wait for either input on the pipe, or for the child to die, or
             * for SIGINT. In the last two cases we hit sigsetjmp's.
             */
            do
            {
                check(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");
                if (!gldb_recv_code(lib_in, &resp))
                {
                    /* Failure here means EOF, which hopefully means that the
                     * program will now terminate.
                     */
                    check(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");
                    sigsuspend(&unblocked);
                    /* Hopefully we won't even reach here, but hit the sigsetjmp. */
                    continue;
                }
                check(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");
            }
            while (!handle_responses(resp));
        }
    }
}

static char *generate_commands(const char *text, int state)
{
    static const command_info *ptr;
    static size_t len;

    if (!state)
    {
        ptr = commands;
        len = strlen(text);
    }

    for (; ptr->name; ptr++)
        if (strncmp(ptr->name, text, len) == 0)
            return bugle_strdup((ptr++)->name);
    return NULL;
}

static char *generate_functions(const char *text, int state)
{
    static budgie_function i;
    static size_t len;

    if (!state)
    {
        len = strlen(text);
        i = 0;
    }
    for (; i < NUMBER_OF_FUNCTIONS; i++)
        if (strncmp(budgie_function_names[i], text, len) == 0)
            return bugle_strdup(budgie_function_names[i++]);
    return NULL;
}

#if HAVE_READLINE
static char **completion(const char *text, int start, int end)
{
    char **matches = NULL;
    const command_data *data;
    char *key;
    int first, last; /* first, last delimit the command, if any */

    /* Complete commands */
    first = 0;
    last = start;
    while (first < start && isspace(rl_line_buffer[first])) first++;
    while (last > first && isspace(rl_line_buffer[last - 1])) last--;
    if (first >= last) /* this is command expansion */
        matches = rl_completion_matches(text, generate_commands);
    else
    {
        /* try to find the command */
        key = bugle_malloc(last - first + 1);
        strncpy(key, rl_line_buffer + first, last - first);
        key[last - first] = '\0';
        data = bugle_hash_get(&command_table, key);
        if (data && data->command && data->command->generator)
        {
            matches = rl_completion_matches(text, data->command->generator);
        }
        free(key);
    }

    rl_attempted_completion_over = 1; /* disable filename expansion */
    return matches;
}
#endif

command_info commands[] =
{
    { "backtrace", "Show a gdb backtrace", true, command_backtrace, NULL },
    { "bt", NULL, true, command_backtrace, NULL },

    { "break", "Set breakpoints", false, command_break_unbreak, generate_functions },
    { "b", NULL, false, command_break_unbreak, generate_functions },
    { "unbreak", "Clear breakpoints", false, command_break_unbreak, generate_functions },

    { "continue", "Continue running the program", true, command_cont, NULL },
    { "c", NULL, true, command_cont, NULL },

    { "chain", "Set the filter-set chain", false, command_chain, NULL },
    { "disable", "Disable a filterset", true, command_enable_disable, NULL },
    { "enable", "Enable a filterset", true, command_enable_disable, NULL },
    { "gdb", "Stop in the program in gdb", false, command_gdb, NULL },
    { "help", "Show the list of commands", false, command_help, generate_commands },
    { "kill", "Kill the program", true, command_kill, NULL },
    { "run", "Start the program", false, command_run, NULL },
    { "state", "Show GL state", true, command_state, NULL }, /* FIXME: completion */
    { "quit", "Exit gldb", false, command_quit, NULL },
    { "screenshot", "Capture a screenshot", true, command_screenshot, NULL },
    { NULL, NULL, false, NULL, NULL }
};

static int compare_commands(const void *a, const void *b)
{
    const command_info *A = (const command_info *) a;
    const command_info *B = (const command_info *) b;

    return strcmp(A->name, B->name);
}

static void sort_commands(void)
{
    const command_info *c;
    size_t count = 0;

    for (c = commands; c->name; c++) count++;
    qsort(commands, count, sizeof(command_info), compare_commands);
}

static void shutdown(void)
{
    bugle_hash_clear(&command_table);
    bugle_hash_clear(&break_on);
    free(prog_argv);
}

static void initialise(void)
{
    const command_info *cmd;

    sort_commands();
    bugle_initialise_hashing();
    bugle_hash_init(&break_on, false);
    bugle_hash_init(&command_table, true);
    for (cmd = commands; cmd->name; cmd++)
        register_command(cmd);
#if HAVE_READLINE
    rl_readline_name = "gldb";
    rl_attempted_completion_function = completion;
#endif
    atexit(shutdown);
}

char **create_argv(int argc, char * const *argv_in)
{
    char **out;
    int i;

    out = bugle_malloc(sizeof(char *) * (argc + 1));
    out[argc] = NULL;
    for (i = 0; i < argc; i++)
        out[i] = argv_in[i];
    return out;
}

int main(int argc, char * const *argv)
{
    if (argc < 2)
    {
        fputs("Usage: gldb <program> <args>\n", stderr);
        return 1;
    }
    prog = argv[1];
    initialise();
    prog_argv = create_argv(argc - 1, argv + 1);
    main_loop();
    return 0;
}
