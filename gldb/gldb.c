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
#if HAVE_READLINE_READLINE_H
# include <readline/readline.h>
# include <readline/history.h>
#endif
#include "src/types.h"
#include "src/canon.h"
#include "common/bool.h"
#include "common/safemem.h"
#include "common/protocol.h"
#include "common/hashtable.h"
/* Uncomment these lines to test the legacy I/O support
#undef HAVE_READLINE_READLINE_H
#define HAVE_READLINE_READLINE_H 0
*/

#if HAVE_READLINE_READLINE_H && !HAVE_RL_COMPLETION_MATCHES
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
static sigjmp_buf chld_env;
/* Started is set to true only after receiving RESP_RUNNING. When
 * we are in the state (running && !started), non-break responses are
 * handled but do not cause a new line to be read.
 */
static bool running = false, started = false;
static pid_t child_pid = -1;
static int child_status = 0; /* status returned from waitpid */
static char *chain = NULL;
static const char *prog = NULL;

/* The table of full command names. This is used for command completion
 * and display of help. There is also a hash table of command names
 * including prefixes. One day it may become a trie. The handler takes the
 * canonical name of the command, the raw line data, and a NULL-terminated
 * array of tokens. The generator is used by readline to complete arguments;
 * see the readline documentation for more details.
 *
 * Please maintain the list in alphabetical order, as it makes the help
 * output look neater.
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
const command_info commands[];

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
        if (chain)
            check(setenv("BUGLE_CHAIN", chain, 1), "setenv");
        else
            unsetenv("BUGLE_CHAIN");
        check(setenv("LD_PRELOAD", LIBDIR "/libbugle.so", 1), "setenv");
        check(setenv("BUGLE_DEBUGGER", "1", 1), "setenv");
        xasprintf(&env, "%d", in_pipe[1]);
        check(setenv("BUGLE_DEBUGGER_FD_OUT", env, 1), "setenv");
        free(env);
        xasprintf(&env, "%d", out_pipe[0]);
        check(setenv("BUGLE_DEBUGGER_FD_IN", env, 1), "setenv");
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
        close(in_pipe[1]);
        close(out_pipe[0]);
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
        fputs("Specify a function or \"error\".\n", stdout);
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
        fputs("Already running\n", stdout);
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
        chain = xstrdup(tokens[1]);
        printf("Chain set to %s.\n", chain);
    }
    if (running)
        fputs("The change will only take effect when the program is restarted.\n", stdout);
    return false;
}

static bool command_help(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    const command_info *c;

    /* FIXME: sort the commands by name */
    for (c = commands; c->name; c++)
        printf("%-12s\t%s\n", c->name, c->help);
    return false;
}

static void register_command(const command_info *cmd)
{
    command_data *data;
    char *key, *end;

    key = xstrdup(cmd->name);
    data = (command_data *) hash_get(&command_table, key);
    if (data)
        assert(!data->root); /* can't redefine commands */
    else
        data = (command_data *) xmalloc(sizeof(command_data));
    data->root = true;
    data->command = cmd;
    hash_set(&command_table, key, data);

    /* Progressively shorten the key to create abbreviations. */
    end = key + strlen(key);
    *--end = '\0';
    while (end > key)
    {
        data = (command_data *) hash_get(&command_table, key);
        if (!data)
        {
            data = (command_data *) xmalloc(sizeof(command_data));
            data->root = false;
            data->command = cmd;
            hash_set(&command_table, key, data);
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
    int form;
    uint32_t resp;

    setup_sigchld();
    while (true)
    {
        if ((form = sigsetjmp(chld_env, 1)) != 0)
        {
            /* If we get here, then the child died. */
            if (form == 1) /* i.e. not from command_backtrace or similar */
                RESTART(waitpid(child_pid, &child_status, 0));
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
            continue;
        }

        if (!running || started)
            handle_commands();

        /* Wait for either input on the pipe, or for the child to die.
         * In the latter case we hit the sigsetjmp above.
         */
        do
        {
            check(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");
            if (!recv_code(lib_in, &resp))
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
            return xstrdup((ptr++)->name);
    return NULL;
}

static char *generate_functions(const char *text, int state)
{
    static size_t i;
    static size_t len;

    if (!state)
    {
        len = strlen(text);
        i = 0;
    }
    for (; i < NUMBER_OF_FUNCTIONS; i++)
        if (strncmp(function_table[i].name, text, len) == 0
            && canonical_function(i) == i)
            return xstrdup(function_table[i++].name);
    return NULL;
}

#if HAVE_READLINE_READLINE_H
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
        key = xmalloc(last - first + 1);
        strncpy(key, rl_line_buffer + first, last - first);
        key[last - first] = '\0';
        data = hash_get(&command_table, key);
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

static void shutdown(void)
{
    hash_clear(&command_table, true);
    hash_clear(&break_on, false);
}

const command_info commands[] =
{
    { "backtrace", "Show a gdb backtrace", true, command_backtrace, NULL },
    { "break", "Set breakpoints", false, command_break_unbreak, generate_functions },
    { "bt", "Alias for backtrace", true, command_backtrace, NULL },
    { "chain", "Set the filter-set chain", false, command_chain, NULL },
    { "continue", "Continue running the program", true, command_cont, NULL },
    { "help", "Show the list of commands", false, command_help, generate_commands },
    { "kill", "Kill the program", true, command_kill, NULL },
    { "run", "Start the program", false, command_run, NULL },
    { "quit", "Exit gldb", false, command_quit, NULL },
    { "unbreak", "Clear breakpoints", false, command_break_unbreak, generate_functions },
    { NULL, NULL, false, NULL }
};

static void initialise(void)
{
    const command_info *cmd;

    initialise_hashing();
    initialise_canonical();
    hash_init(&break_on);
    hash_init(&command_table);
    for (cmd = commands; cmd->name; cmd++)
        register_command(cmd);
#if HAVE_READLINE_READLINE_H
    rl_readline_name = "gldb";
    rl_attempted_completion_function = completion;
#endif
    atexit(shutdown);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fputs("Usage: gldb <program>\n", stderr);
        return 1;
    }
    prog = argv[1];
    initialise();
    main_loop();
    return 0;
}
