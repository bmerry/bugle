/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "common/bool.h"
#include "common/protocol.h"
#include "common/safemem.h"
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "gldb/gldb-common.h"

static int lib_in, lib_out;
/* Started is set to true only after receiving RESP_RUNNING. When
 * we are in the state (running && !started), non-break responses are
 * handled but do not cause a new line to be read.
 */
static bool running = false, started = false;
static pid_t child_pid = -1;
static char *filter_chain = NULL;
static const char *prog = NULL;
static char **prog_argv = NULL;

static gldb_state *state_root = NULL;
static bool state_dirty = true;

static bool break_on_error = true;
static bugle_hash_table break_on;

/* Spawns off the program, and returns the pid */
static pid_t execute(void (*child_init)(void))
{
    pid_t pid;
    /* in/out refers to our view, not child view */
    int in_pipe[2], out_pipe[2];
    char *env;

    gldb_safe_syscall(pipe(in_pipe), "pipe");
    gldb_safe_syscall(pipe(out_pipe), "pipe");
    switch ((pid = fork()))
    {
    case -1:
        perror("fork failed");
        exit(1);
    case 0: /* Child */
        (*child_init)();

        if (filter_chain)
            gldb_safe_syscall(setenv("BUGLE_CHAIN", filter_chain, 1), "setenv");
        else
            unsetenv("BUGLE_CHAIN");
        gldb_safe_syscall(setenv("LD_PRELOAD", LIBDIR "/libbugle.so", 1), "setenv");
        gldb_safe_syscall(setenv("BUGLE_DEBUGGER", "1", 1), "setenv");
        bugle_asprintf(&env, "%d", in_pipe[1]);
        gldb_safe_syscall(setenv("BUGLE_DEBUGGER_FD_OUT", env, 1), "setenv");
        free(env);
        bugle_asprintf(&env, "%d", out_pipe[0]);
        gldb_safe_syscall(setenv("BUGLE_DEBUGGER_FD_IN", env, 1), "setenv");
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

static void state_destroy(gldb_state *s)
{
    bugle_list_node *i;

    for (i = bugle_list_head(&s->children); i; i = bugle_list_next(i))
        state_destroy((gldb_state *) bugle_list_data(i));
    bugle_list_clear(&s->children);
    if (s->name) free(s->name);
    if (s->value) free(s->value);
    free(s);
}

static gldb_state *state_get(void)
{
    gldb_state *s, *child;
    uint32_t resp;

    s = bugle_malloc(sizeof(gldb_state));
    /* The list actually does own the memory, but we do the free as
     * part of state_destroy.
     */
    bugle_list_init(&s->children, false);
    gldb_protocol_recv_string(lib_in, &s->name);
    gldb_protocol_recv_string(lib_in, &s->value);

    do
    {
        gldb_protocol_recv_code(lib_in, &resp);
        switch (resp)
        {
        case RESP_STATE_NODE_BEGIN:
            child = state_get();
            bugle_list_append(&s->children, child);
            break;
        case RESP_STATE_NODE_END:
            break;
        default:
            fprintf(stderr, "Warning: unexpected code in state tree\n");
        }
    } while (resp != RESP_STATE_NODE_END);

    return s;
}

gldb_state *gldb_state_update()
{
    uint32_t resp;

    if (state_dirty)
    {
        gldb_protocol_send_code(lib_out, REQ_STATE_TREE);
        gldb_protocol_recv_code(lib_in, &resp);
        switch (resp)
        {
        case RESP_STATE_NODE_BEGIN:
            if (state_root) state_destroy(state_root);
            state_root = state_get();
            state_dirty = false;
            break;
        default:
            fprintf(stderr, "Unexpected response %#08x\n", resp);
        }
    }
    return state_root;
}

/* Only first n characters of name are considered. This simplifies
 * generate_commands.
 */
gldb_state *gldb_state_find(gldb_state *root, const char *name, size_t n)
{
    const char *split;
    bugle_list_node *i;
    gldb_state *child;
    bool found;

    if (n > strlen(name)) n = strlen(name);
    while (n > 0)
    {
        found = false;
        split = strchr(name, '.');
        while (split == name && n > 0 && name[0] == '.')
        {
            name++;
            n--;
            split = strchr(name, '.');
        }
        if (split == NULL || split > name + n) split = name + n;

        for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
        {
            child = (gldb_state *) bugle_list_data(i);
            if (child->name && strncmp(child->name, name, split - name) == 0
                && child->name[split - name] == '\0')
            {
                found = true;
                root = child;
                n -= split - name;
                name = split;
                break;
            }
        }
        if (!found) return NULL;
    }
    return root;
}

/* Checks that the result of a system call is not -1, otherwise throws
 * an error.
 */
void gldb_safe_syscall(int r, const char *str)
{
    if (r == -1)
    {
        perror(str);
        exit(1);
    }
}

void gldb_run(void (*child_init)(void))
{
    const bugle_hash_entry *h;

    child_pid = execute(child_init);
    /* Send breakpoints */
    gldb_protocol_send_code(lib_out, REQ_BREAK_ERROR);
    gldb_protocol_send_code(lib_out, break_on_error ? 1 : 0);
    for (h = bugle_hash_begin(&break_on); h; h = bugle_hash_next(&break_on, h))
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK);
        gldb_protocol_send_string(lib_out, h->key);
        gldb_protocol_send_code(lib_out, *(const char *) h->value - '0');
    }
    gldb_protocol_send_code(lib_out, REQ_RUN);
    running = true;
    started = false;
}

void gldb_send_continue(void)
{
    gldb_protocol_send_code(lib_out, REQ_CONT);
}

void gldb_send_quit(void)
{
    gldb_protocol_send_code(lib_out, REQ_QUIT);
}

void gldb_send_enable_disable(const char *filterset, bool enable)
{
    gldb_protocol_send_code(lib_out, enable ? REQ_ENABLE_FILTERSET : REQ_DISABLE_FILTERSET);
    gldb_protocol_send_string(lib_out, filterset);
}

void gldb_send_screenshot(void)
{
    gldb_protocol_send_code(lib_out, REQ_SCREENSHOT);
}

void gldb_send_async(void)
{
    gldb_protocol_send_code(lib_out, REQ_ASYNC);
}

void gldb_set_break_error(bool brk)
{
    break_on_error = brk;
    if (running)
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK_ERROR);
        gldb_protocol_send_code(lib_out, brk ? 1 : 0);
    }
}

void gldb_set_break(const char *function, bool brk)
{
    bugle_hash_set(&break_on, function, brk ? "1" : "0");
    if (running)
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK);
        gldb_protocol_send_string(lib_out, function);
        gldb_protocol_send_code(lib_out, brk ? 1 : 0);
    }
}

void gldb_set_chain(const char *chain)
{
    if (filter_chain) free(filter_chain);
    if (chain) filter_chain = bugle_strdup(chain);
    else chain = NULL;
}

void gldb_info_stopped(void)
{
    state_dirty = true;
}

void gldb_info_running(void)
{
    started = true;
    state_dirty = true;
}

void gldb_info_child_terminated(void)
{
    running = false;
    started = false;
    close(lib_in);
    close(lib_out);
    lib_in = -1;
    lib_out = -1;
    child_pid = 0;
}

bool gldb_running(void)
{
    return running;
}

bool gldb_started(void)
{
    return started;
}

pid_t gldb_child_pid(void)
{
    return child_pid;
}

const char *gldb_program(void)
{
    return prog;
}

/* FIXME: make this unnecessary */
int gldb_in_pipe(void)
{
    return lib_in;
}

void gldb_initialise(int argc, char * const *argv)
{
    int i;

    prog = argv[1];
    prog_argv = bugle_malloc(sizeof(char *) * argc);
    prog_argv[argc - 1] = NULL;
    for (i = 1; i < argc; i++)
        prog_argv[i - 1] = argv[i];

    bugle_hash_init(&break_on, false);
}

void gldb_shutdown(void)
{
    bugle_hash_clear(&break_on);
    free(prog_argv);
}
