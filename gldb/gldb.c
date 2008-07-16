/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#if HAVE_READLINE
# include <readline/readline.h>
# include <readline/history.h>
#endif
#include <bugle/misc.h>
#include <bugle/hashtable.h>
#include <budgie/reflect.h>
#include "common/protocol.h"
#include "gldb/gldb-common.h"
#include "xalloc.h"
#include "xvasprintf.h"
/* Uncomment these lines to test the legacy I/O support
#undef HAVE_READLINE
#define HAVE_READLINE
*/

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
static hash_table command_table;
static char *screenshot_file = NULL;
static int chld_pipes[2], int_pipes[2];
static sigset_t blocked, unblocked;

static void sigchld_handler(int sig)
{
    char buf = 'c';
    write(chld_pipes[1], &buf, 1);
}

static void sigint_handler(int sig)
{
    char buf = 'i';
    write(int_pipes[1], &buf, 1);
}

#if !HAVE_READLINE
static void chop(char *s)
{
    size_t len = strlen(s);
    while (len && isspace(s[len - 1]))
        s[--len] = '\0';
}
#endif

/* Returns true on quit, false otherwise */
static bool handle_commands(void)
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
        gldb_safe_syscall(sigaction(SIGINT, &act, &old_act), "sigaction");
        sigemptyset(&sigint_set);
        sigaddset(&sigint_set, SIGINT);
        gldb_safe_syscall(sigprocmask(SIG_UNBLOCK, &sigint_set, NULL), "sigprocmask");

        line = readline("(gldb) ");

        gldb_safe_syscall(sigprocmask(SIG_BLOCK, &sigint_set, NULL), "sigprocmask");
        gldb_safe_syscall(sigaction(SIGINT, &old_act, NULL), "sigaction");
        if (line && *line) add_history(line);
#else
        fputs("(gldb) ", stdout);
        fflush(stdout);
        if ((line = bugle_afgets(stdin)) != NULL)
            chop(line);
#endif
        if (!line)
        {
            if (gldb_get_status() != GLDB_STATUS_DEAD) gldb_send_quit(0);
            if (prev_line) free(prev_line);
            return true;
        }
        else
        {
            if (!*line && prev_line != NULL)
            {
                free(line);
                line = xstrdup(prev_line);
            }
            if (*line)
            {
                if (prev_line) free(prev_line);
                prev_line = xstrdup(line);

                /* Tokenise */
                num_tokens = 0;
                for (cur = line; *cur; cur++)
                    if (!isspace(*cur)
                        && (cur == line || isspace(cur[-1])))
                        num_tokens++;
                if (num_tokens)
                {
                    tokens = XNMALLOC(num_tokens + 1, char *);
                    tokens[num_tokens] = NULL;
                    i = 0;
                    for (cur = line; *cur; cur++)
                    {
                        if (!isspace(*cur))
                        {
                            base = cur;
                            while (*cur && !isspace(*cur)) cur++;
                            tokens[i] = XNMALLOC(cur - base + 1, char);
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
                    else if (data->command->running && gldb_get_status() == GLDB_STATUS_DEAD)
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
    return false;
}

/* Deals with response code resp. Returns true if we are done processing
 * responses, or false if we are expecting more.
 */
static int handle_responses(void)
{
    gldb_response *r;
    gldb_response_data_framebuffer *fb;

    FILE *f;
    uint32_t i;

    r = gldb_get_response();
    /* For some reason select() sometimes claims that there is data
     * available when in fact the pipe has been closed. In this case
     * gldb_get_response will return NULL (for EOF), and we go back to the
     * loop to await SIGCHLD notification.
     */
    if (!r) return 2;
    switch (r->code)
    {
    case RESP_BREAK:
        printf("Break on %s\n", ((gldb_response_break *) r)->call);
        break;
    case RESP_BREAK_ERROR:
        printf("Error %s in %s\n",
               ((gldb_response_break_error *) r)->error,
               ((gldb_response_break_error *) r)->call);
        break;
    case RESP_ERROR:
        printf("%s\n", ((gldb_response_break_error *) r)->error);
        break;
    case RESP_RUNNING:
        printf("Running.\n");
        return 0;
    case RESP_DATA:
        if (!screenshot_file)
        {
            fputs("Unexpected screenshot data. This is a bug.\n", stderr);
            break;
        }
        fb = (gldb_response_data_framebuffer *) r;
        if (fb->subtype != REQ_DATA_FRAMEBUFFER)
        {
            fputs("Unexpected non-screenshot data. This is a bug.\n", stderr);
            break;
        }

        f = fopen(screenshot_file, "wb");
        if (!f)
            fprintf(stderr, "Cannot open %s: %s\n", screenshot_file, strerror(errno));
        else
        {
            fprintf(f, "P6\n# Captured by gldb\n%d %d\n255\n",
                    fb->width, fb->height);
            for (i = 0; i < fb->height; i++)
                if (fwrite(fb->data + (fb->height - 1 - i) * fb->width * 3,
                           1, fb->width * 3, f) != fb->width * 3)
                {
                    fprintf(stderr, "Error writing %s: %s\n", screenshot_file, strerror(errno));
                    break;
                }
            if (fclose(f) == EOF && i == fb->height)
                fprintf(stderr, "Error writing %s: %s\n", screenshot_file, strerror(errno));
        }
        free(screenshot_file);
        screenshot_file = NULL;
        break;
    }
    gldb_free_response(r);

    return (gldb_get_status() == GLDB_STATUS_RUNNING
            || gldb_get_status() == GLDB_STATUS_STOPPED) ? 1 : 0;
}

static void setup_signals(void)
{
    struct sigaction act;

    /* In normal operation, we block SIGCHLD to avoid race conditions.
     * We explicitly unblock it only when waiting for things to happen.
     */
    gldb_safe_syscall(sigprocmask(SIG_BLOCK, NULL, &unblocked), "sigprocmask");
    gldb_safe_syscall(sigprocmask(SIG_BLOCK, NULL, &blocked), "sigprocmask");
    sigaddset(&blocked, SIGCHLD);
    sigaddset(&blocked, SIGINT);
    gldb_safe_syscall(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");

    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGINT);
    gldb_safe_syscall(sigaction(SIGCHLD, &act, NULL), "sigaction");

    act.sa_handler = sigint_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGCHLD);
    gldb_safe_syscall(sigaction(SIGINT, &act, NULL), "sigaction");

    /* We do various things with process groups, and gdb does some more.
     * To prevent any embarrassing stoppages we simply allow everyone
     * to use the terminal. The only case where this might blow up is
     * if gldb is backgrounded by the shell.
     */
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    gldb_safe_syscall(sigaction(SIGTTIN, &act, NULL), "sigaction");
    gldb_safe_syscall(sigaction(SIGTTOU, &act, NULL), "sigaction");
}

static void main_loop(void)
{
    fd_set readfds;
    int n;
    int result;
    int status;
    pid_t pid;
    bool done, pipe_done;

    while (!handle_commands())
    {
        done = false;
        pipe_done = false;
        while (gldb_get_status() != GLDB_STATUS_DEAD && !done)
        {
            /* Allow signals in. This appears to open a race condition (because
             * a signal may arrive between now and select), but the write() in
             * the signal handlers will take care of it.
             */
            gldb_safe_syscall(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");

            n = 0;
            FD_ZERO(&readfds);
            FD_SET(chld_pipes[0], &readfds);
            if (chld_pipes[0] >= n) n = chld_pipes[0] + 1;
            FD_SET(int_pipes[0], &readfds);
            if (int_pipes[0] >= n) n = int_pipes[0] + 1;
            if (!pipe_done)
            {
                FD_SET(gldb_get_in_pipe(), &readfds);
                if (gldb_get_in_pipe() >= n) n = gldb_get_in_pipe() + 1;
            }
            result = select(n, &readfds, NULL, NULL, NULL);
            if (result == -1 && errno != EINTR)
            {
                perror("select");
                exit(1);
            }

            gldb_safe_syscall(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");

            /* If the child pipe was flagged by select, we need to drain it
             * to avoid spinning on it. We don't take the child pipe as
             * proof that the child as died, since it may indicate that
             * gdb has exited.
             */
            if (result != -1 && FD_ISSET(chld_pipes[0], &readfds))
            {
                char buf;
                read(chld_pipes[0], &buf, 1);
            }

            /* Find out what happened */
            pid = waitpid(gldb_get_child_pid(), &status, WNOHANG);
            gldb_safe_syscall(pid, "waitpid");
            if (pid == gldb_get_child_pid())
            {
                /* Child terminated */
                if (WIFEXITED(status))
                {
                    if (WEXITSTATUS(status) == 0)
                        printf("Program exited normally.\n");
                    else
                        printf("Program exited with return code %d.\n", WEXITSTATUS(status));
                }
                else if (WIFSIGNALED(status))
                    printf("Program was terminated with signal %d.\n", WTERMSIG(status));
                else
                    printf("Program was terminated abnormally.\n");

                assert(gldb_get_status() != GLDB_STATUS_DEAD);
                gldb_notify_child_dead();
                done = true;
            }
            else if (result != -1 && FD_ISSET(int_pipes[0], &readfds))
            {
                char buf;

                /* Ctrl-C was pressed */
                read(int_pipes[0], &buf, 1);
                gldb_send_async(0);
                done = false; /* We still need to wait for RESP_STOP */
            }
            else if (result != -1 && FD_ISSET(gldb_get_in_pipe(), &readfds))
            {
                /* Response from child */
                switch (handle_responses())
                {
                case 0: break;                    /* Expecting more */
                case 1: done = true; break;       /* Get commands */
                case 2: pipe_done = true; break;  /* EOF on pipe, expect SIGCHLD */
                }
            }
        }
    }
}

static void make_indent(int indent, FILE *out)
{
    int i;
    for (i = 0; i < indent; i++)
        fputc(' ', out);
}

static void state_dump(const gldb_state *root, int depth)
{
    linked_list_node *i;
    char *str;

    if (root->name && root->name[0])
    {
        make_indent(depth * 4, stdout);
        fputs(root->name, stdout);
        str = gldb_state_string(root);
        if (str && str[0])
            printf(" = %s\n", str);
        else
            fputc('\n', stdout);
        free(str);
    }
    if (bugle_list_head(&root->children))
    {
        make_indent(depth * 4, stdout);
        printf("{\n");
        for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
            state_dump((const gldb_state *) bugle_list_data(i), depth + 1);
        make_indent(depth * 4, stdout);
        printf("}\n");
    }
}

static bool command_cont(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    gldb_send_continue(0);
    return true;
}

static bool command_step(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    gldb_send_step(0);
    return true;
}

static bool command_kill(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    gldb_send_quit(0);
    return true;
}

static bool command_quit(const char *cmd,
                         const char *line,
                         const char * const *tokens)
{
    if (gldb_get_status() != GLDB_STATUS_DEAD) gldb_send_quit(0);
    exit(0);
}

static bool command_break_unbreak(const char *cmd,
                                  const char *line,
                                  const char * const *tokens)
{
    const char *func;

    func = tokens[1];
    if (!func)
    {
        fputs("Specify a function or \"error\".\n", stdout);
        return false;
    }

    if (strcmp(func, "error") == 0)
        gldb_set_break_error(0, cmd[0] == 'b');
    else
        gldb_set_break(0, func, cmd[0] == 'b');
    return gldb_get_status() != GLDB_STATUS_DEAD;
}

static void child_init(void)
{
    /* We don't want the child to receive Ctrl-C, but we want to
     * allow it to write to the terminal. We move it into a background
     * process group, and it inherits SIG_IGN on SIGTTIN and SIGTTOU.
     *
     * However, we don't want the child to have SIGINT and SIGCHLD
     * blocked.
     */
    setpgid(0, 0);
    sigprocmask(SIG_SETMASK, &unblocked, NULL);
}

static bool command_run(const char *cmd,
                        const char *line,
                        const char * const *tokens)
{
    if (gldb_get_status() != GLDB_STATUS_DEAD)
    {
        fputs("Already running\n", stdout);
        return false;
    }
    else
    {
        gldb_run(0, child_init);
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

    gldb_safe_syscall(pipe(in_pipe), "pipe");
    gldb_safe_syscall(pipe(out_pipe), "pipe");
    gldb_safe_syscall(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");

    switch (pid = fork())
    {
    case -1:
        perror("fork");
        exit(1);
    case 0: /* child */
        sigprocmask(SIG_SETMASK, &unblocked, NULL);
        pid_str = xasprintf("%lu", (unsigned long) gldb_get_child_pid());
        /* FIXME: if in_pipe[1] or out_pipe[0] is already 0 or 1 */
        gldb_safe_syscall(dup2(in_pipe[1], 1), "dup2");
        gldb_safe_syscall(dup2(out_pipe[0], 0), "dup2");
        if (in_pipe[0] != 0 && in_pipe[0] != 1) gldb_safe_syscall(close(in_pipe[0]), "close");
        if (in_pipe[1] != 0 && in_pipe[1] != 1) gldb_safe_syscall(close(in_pipe[1]), "close");
        if (out_pipe[0] != 0 && out_pipe[0] != 1) gldb_safe_syscall(close(out_pipe[0]), "close");
        if (out_pipe[0] != 1 && out_pipe[1] != 1) gldb_safe_syscall(close(out_pipe[1]), "close");
        execlp("gdb", "gdb", "-batch", "-nx", "-command", "/dev/stdin", "-p", pid_str, NULL);
        perror("could not invoke gdb");
        free(pid_str);
        exit(1);
    default: /* parent */
        gldb_safe_syscall(close(in_pipe[1]), "close");
        gldb_safe_syscall(close(out_pipe[0]), "close");
        gldb_safe_syscall((in = fdopen(in_pipe[0], "r")) ? 0 : -1, "fdopen");
        gldb_safe_syscall((out = fdopen(out_pipe[1], "w")) ? 0 : -1, "fdopen");
        fprintf(out, "set height 0\nbacktrace\nquit\n");
        fflush(out);
        while ((ln = bugle_afgets(in)) != NULL)
        {
            /* strip out the rubbish */
            if (*ln == '#' || *ln == ' ') fputs(ln, stdout);
            free(ln);
        }
        RESTART(waitpid(pid, &status, 0));
        /* We don't drain chld_pipe, in case we accidentally miss the
         * real thing. The main loop is protected against spurious
         * stoppages.
         */
    }

    gldb_safe_syscall(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");
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
    if (strcmp(tokens[1], "none") == 0)
    {
        gldb_program_set_setting(GLDB_PROGRAM_SETTING_CHAIN, NULL);
        fputs("Chain cleared.\n", stdout);
    }
    else
    {
        gldb_program_set_setting(GLDB_PROGRAM_SETTING_CHAIN, tokens[1]);
        printf("Chain set to %s.\n", tokens[1]);
    }
    if (gldb_get_status() != GLDB_STATUS_DEAD)
        fputs("The change will only take effect when the program is restarted.\n", stdout);
    return false;
}

static bool command_enable_disable(const char *cmd,
                                   const char *line,
                                   const char * const *tokens)
{
    if (!tokens[1] || !*tokens[1])
    {
        fputs("Usage: enable|disable <filterset>\n", stdout);
        return false;
    }

    gldb_send_enable_disable(0, tokens[1], strcmp(cmd, "enable") == 0);
    return true;
}

static bool command_gdb(const char *cmd,
                        const char *line,
                        const char * const *tokens)
{
#if BUGLE_OSAPI_POSIX
    pid_t pid, pgrp, tc_pgrp;
    char *pid_str;
    int status;
    bool fore;

    gldb_safe_syscall(sigprocmask(SIG_SETMASK, &unblocked, NULL), "sigprocmask");

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

        pid_str = xasprintf("%lu", (unsigned long) gldb_get_child_pid());
        execlp("gdb", "gdb", "-p", pid_str, NULL);
        perror("could not invoke gdb");
        free(pid_str);
        exit(1);
    default: /* parent */
        RESTART(waitpid(pid, &status, 0));
        /* Reclaim foreground */
        if (fore) tcsetpgrp(1, pgrp);
    }

    gldb_safe_syscall(sigprocmask(SIG_SETMASK, &blocked, NULL), "sigprocmask");
#else
    printf("gdb command not implemented on this platform\n");
#endif
    return false;
}

static bool command_state(const char *cmd,
                          const char *line,
                          const char * const *tokens)
{
    gldb_state *s;
    gldb_state *root;

    root = gldb_state_update();
    if (!tokens[1]) s = root;
    else
    {
        s = gldb_state_find(root, tokens[1], strlen(tokens[1]));
        if (!s)
            fprintf(stderr, "No such state %s\n", tokens[1]);
    }
    if (s) state_dump(s, 0);
    return false;
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
    if (screenshot_file) free(screenshot_file);
    screenshot_file = xstrdup(tokens[1]);
    gldb_send_data_framebuffer(0, 0, 0, GL_FRONT, GL_RGB, GL_UNSIGNED_BYTE);
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

    key = xstrdup(cmd->name);
    data = (command_data *) bugle_hash_get(&command_table, key);
    if (data)
        assert(!data->root); /* can't redefine commands */
    else
        data = XMALLOC(command_data);
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
            data = XMALLOC(command_data);
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
    static budgie_function i;
    static size_t len;

    if (!state)
    {
        len = strlen(text);
        i = 0;
    }
    for (; i < budgie_function_count(); i++)
        if (strncmp(budgie_function_name(i), text, len) == 0)
            return xstrdup(budgie_function_name(i++));
    return NULL;
}

static char *generate_state(const char *text, int state)
{
    static const gldb_state *root; /* total state we have */
    static linked_list_node *node;  /* walk of children */
    static const char *split;
    gldb_state *state_root;
    const gldb_state *child;
    char *ans;

    if (gldb_get_status() != GLDB_STATUS_STOPPED) return NULL;
    state_root = gldb_state_update();
    if (!state_root) return NULL;
    if (!state)
    {
        root = NULL;
        node = NULL;

        split = strrchr(text, '.');
        if (!split)
        {
            root = state_root;
            split = text;
        }
        else
        {
            root = gldb_state_find(state_root, text, split - text);
            split++;
            if (!root) return NULL;
        }
        node = bugle_list_head(&root->children);
    }

    for (; node; node = bugle_list_next(node))
    {
        child = (const gldb_state *) bugle_list_data(node);
        if (child->name && strncmp(child->name, split, strlen(split)) == 0)
        {
            node = bugle_list_next(node);
            ans = XNMALLOC(strlen(child->name) + (split - text) + 1, char);
            strncpy(ans, text, split - text);
            strcpy(ans + (split - text), child->name);
            return ans;
        }
    }
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
        key = XNMALLOC(last - first + 1, char);
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
    { "state", "Show GL state", true, command_state, generate_state },

    { "step", "Continue until next OpenGL call", true, command_step, NULL },
    { "s", NULL, true, command_step, NULL },

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

static void cleanup(void)
{
    gldb_shutdown();

    bugle_hash_clear(&command_table);
}

static void initialise(void)
{
    const command_info *cmd;

    sort_commands();
    pipe(chld_pipes);
    pipe(int_pipes);
    bugle_hash_init(&command_table, free);
    for (cmd = commands; cmd->name; cmd++)
        register_command(cmd);
#if HAVE_READLINE
    rl_readline_name = "gldb";
    rl_attempted_completion_function = completion;
#endif
    atexit(cleanup);
}

int main(int argc, const char * const *argv)
{
    if (argc < 2)
    {
        fputs("Usage: gldb <program> <args>\n", stderr);
        return 1;
    }
    initialise();
    gldb_initialise(argc, argv);
    setup_signals();
    main_loop();
    return 0;
}
