/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
static gldb_status status = GLDB_STATUS_DEAD;
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

void set_status(gldb_status s)
{
    if (status == s) return;
    status = s;
    switch (status)
    {
    case GLDB_STATUS_RUNNING:
    case GLDB_STATUS_STOPPED:
        state_dirty = true;
        break;
    case GLDB_STATUS_STARTED:
        break;
    case GLDB_STATUS_DEAD:
        close(lib_in);
        close(lib_out);
        lib_in = -1;
        lib_out = -1;
        child_pid = 0;
        break;
    }
}

gldb_state *gldb_state_update(uint32_t id)
{
    gldb_response *r = NULL;

    if (state_dirty)
    {
        gldb_send_state_tree(0);
        do
        {
            if (r) gldb_free_response(r);
            r = gldb_get_response();
        } while (r && r->code != RESP_STATE_NODE_BEGIN);
        if (!r)
            state_root = NULL;
        else
        {
            state_root = ((gldb_response_state_tree *) r)->root;
            free(r);
        }
        state_dirty = false;
    }
    return state_root;
}

static gldb_response *gldb_get_response_ans(uint32_t code, uint32_t id)
{
    gldb_response_ans *r;

    r = bugle_malloc(sizeof(gldb_response_ans));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_code(lib_in, &r->value);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_stop(uint32_t code, uint32_t id)
{
    gldb_response_stop *r;

    set_status(GLDB_STATUS_STOPPED);
    r = bugle_malloc(sizeof(gldb_response_stop));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->call);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_break(uint32_t code, uint32_t id)
{
    gldb_response_break *r;

    set_status(GLDB_STATUS_STOPPED);
    r = bugle_malloc(sizeof(gldb_response_break));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->call);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_break_error(uint32_t code, uint32_t id)
{
    gldb_response_break_error *r;

    set_status(GLDB_STATUS_STOPPED);
    r = bugle_malloc(sizeof(gldb_response_break_error));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->call);
    gldb_protocol_recv_string(lib_in, &r->error);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_state(uint32_t code, uint32_t id)
{
    gldb_response_state *r;

    r = bugle_malloc(sizeof(gldb_response_state));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->state);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_error(uint32_t code, uint32_t id)
{
    gldb_response_error *r;

    r = bugle_malloc(sizeof(gldb_response_error));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_code(lib_in, &r->error_code);
    gldb_protocol_recv_string(lib_in, &r->error);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_running(uint32_t code, uint32_t id)
{
    gldb_response_running *r;

    set_status(GLDB_STATUS_RUNNING);
    r = bugle_malloc(sizeof(gldb_response_running));
    r->code = code;
    r->id = id;
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_screenshot(uint32_t code, uint32_t id)
{
    gldb_response_screenshot *r;

    r = bugle_malloc(sizeof(gldb_response_screenshot));
    r->code = code;
    r->id = id;
    gldb_protocol_recv_binary_string(lib_in, &r->length, &r->data);
    return (gldb_response *) r;
}

static gldb_state *state_get(void)
{
    gldb_state *s, *child;
    uint32_t resp, id;
    uint32_t numeric_name, enum_name;

    s = bugle_malloc(sizeof(gldb_state));
    /* The list actually does own the memory, but we do the free as
     * part of state_destroy.
     */
    bugle_list_init(&s->children, false);
    gldb_protocol_recv_string(lib_in, &s->name);
    gldb_protocol_recv_code(lib_in, &numeric_name);
    gldb_protocol_recv_code(lib_in, &enum_name);
    s->numeric_name = numeric_name;
    s->enum_name = enum_name;
    gldb_protocol_recv_string(lib_in, &s->value);

    do
    {
        gldb_protocol_recv_code(lib_in, &resp);
        gldb_protocol_recv_code(lib_in, &id);
        switch (resp)
        {
        case RESP_STATE_NODE_BEGIN:
            child = state_get();
            bugle_list_append(&s->children, child);
            break;
        case RESP_STATE_NODE_END:
            break;
        default:
            fprintf(stderr, "Unexpected code %lu in state tree\n",
                   (unsigned long) resp);
            exit(1);
        }
    } while (resp != RESP_STATE_NODE_END);

    return s;
}

static gldb_response *gldb_get_response_state_tree(uint32_t code, uint32_t id)
{
    gldb_response_state_tree *r;

    r = bugle_malloc(sizeof(gldb_response_state_tree));
    r->code = code;
    r->id = id;
    r->root = state_get();
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_texture(uint32_t code, uint32_t id,
                                                     uint32_t subtype,
                                                     uint32_t length, char *data)
{
    gldb_response_data_texture *r;

    r = bugle_malloc(sizeof(gldb_response_data_texture));
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    gldb_protocol_recv_code(lib_in, &r->width);
    gldb_protocol_recv_code(lib_in, &r->height);
    gldb_protocol_recv_code(lib_in, &r->depth);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_framebuffer(uint32_t code, uint32_t id,
                                                         uint32_t subtype,
                                                         uint32_t length, char *data)
{
    gldb_response_data_framebuffer *r;

    r = bugle_malloc(sizeof(gldb_response_data_texture));
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    gldb_protocol_recv_code(lib_in, &r->width);
    gldb_protocol_recv_code(lib_in, &r->height);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_shader(uint32_t code, uint32_t id,
                                                    uint32_t subtype,
                                                    uint32_t length, char *data)
{
    gldb_response_data_shader *r;

    r = bugle_malloc(sizeof(gldb_response_data_shader));
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data(uint32_t code, uint32_t id)
{
    uint32_t subtype;
    uint32_t length;
    char *data;

    gldb_protocol_recv_code(lib_in, &subtype);
    gldb_protocol_recv_binary_string(lib_in, &length, &data);
    switch (subtype)
    {
    case REQ_DATA_TEXTURE:
        return gldb_get_response_data_texture(code, id, subtype, length, data);
    case REQ_DATA_FRAMEBUFFER:
        return gldb_get_response_data_framebuffer(code, id, subtype, length, data);
    case REQ_DATA_SHADER:
        return gldb_get_response_data_shader(code, id, subtype, length, data);
    default:
        fprintf(stderr, "Unknown DATA subtype %lu\n", (unsigned long) subtype);
        exit(1);
    }
}

gldb_response *gldb_get_response(void)
{
    uint32_t code, id;

    if (!gldb_protocol_recv_code(lib_in, &code)
        || !gldb_protocol_recv_code(lib_in, &id))
        return NULL;

    switch (code)
    {
    case RESP_ANS: return gldb_get_response_ans(code, id);
    case RESP_STOP: return gldb_get_response_stop(code, id);
    case RESP_BREAK: return gldb_get_response_break(code, id);
    case RESP_BREAK_ERROR: return gldb_get_response_break_error(code, id);
    case RESP_STATE: return gldb_get_response_state(code, id);
    case RESP_ERROR: return gldb_get_response_error(code, id);
    case RESP_RUNNING: return gldb_get_response_running(code, id);
    case RESP_SCREENSHOT: return gldb_get_response_screenshot(code, id);
    case RESP_STATE_NODE_BEGIN: return gldb_get_response_state_tree(code, id);
    case RESP_DATA: return gldb_get_response_data(code, id);
    default:
        fprintf(stderr, "Unexpected response %#08x\n", code);
        return NULL;
    }
}

void gldb_free_response(gldb_response *r)
{
    switch (r->code)
    {
    case RESP_BREAK:
        free(((gldb_response_break *) r)->call);
        break;
    case RESP_BREAK_ERROR:
        free(((gldb_response_break_error *) r)->call);
        free(((gldb_response_break_error *) r)->error);
        break;
    case RESP_STATE:
        free(((gldb_response_state *) r)->state);
        break;
    case RESP_ERROR:
        free(((gldb_response_error *) r)->error);
        break;
    case RESP_SCREENSHOT:
        free(((gldb_response_screenshot *) r)->data);
        break;
    case RESP_STATE_NODE_BEGIN:
        state_destroy(((gldb_response_state_tree *) r)->root);
        break;
    case RESP_DATA:
        free(((gldb_response_data *) r)->data);
        break;
    }
    free(r);
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

gldb_state *gldb_state_find_child_numeric(gldb_state *parent, GLint name)
{
    gldb_state *child;
    bugle_list_node *i;

    /* FIXME: build indices on the state */
    for (i = bugle_list_head(&parent->children); i; i = bugle_list_next(i))
    {
        child = (gldb_state *) bugle_list_data(i);
        if (child->numeric_name == name) return child;
    }
    return NULL;
}

gldb_state *gldb_state_find_child_enum(gldb_state *parent, GLenum name)
{
    gldb_state *child;
    bugle_list_node *i;

    /* FIXME: build indices on the state */
    for (i = bugle_list_head(&parent->children); i; i = bugle_list_next(i))
    {
        child = (gldb_state *) bugle_list_data(i);
        if (child->enum_name == name) return child;
    }
    return NULL;
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

void gldb_run(uint32_t id, void (*child_init)(void))
{
    const bugle_hash_entry *h;

    child_pid = execute(child_init);
    /* Send breakpoints */
    gldb_protocol_send_code(lib_out, REQ_BREAK_ERROR);
    gldb_protocol_send_code(lib_out, 0);
    gldb_protocol_send_code(lib_out, break_on_error ? 1 : 0);
    for (h = bugle_hash_begin(&break_on); h; h = bugle_hash_next(&break_on, h))
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK);
        gldb_protocol_send_code(lib_out, 0);
        gldb_protocol_send_string(lib_out, h->key);
        gldb_protocol_send_code(lib_out, *(const char *) h->value - '0');
    }
    gldb_protocol_send_code(lib_out, REQ_RUN);
    gldb_protocol_send_code(lib_out, id);
    set_status(GLDB_STATUS_STARTED);
}

void gldb_send_continue(uint32_t id)
{
    assert(status == GLDB_STATUS_STOPPED);
    set_status(GLDB_STATUS_RUNNING);
    gldb_protocol_send_code(lib_out, REQ_CONT);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_step(uint32_t id)
{
    assert(status == GLDB_STATUS_STOPPED);
    set_status(GLDB_STATUS_RUNNING);
    gldb_protocol_send_code(lib_out, REQ_STEP);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_quit(uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_QUIT);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_enable_disable(uint32_t id, const char *filterset, bool enable)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, enable ? REQ_ACTIVATE_FILTERSET : REQ_DEACTIVATE_FILTERSET);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_string(lib_out, filterset);
}

void gldb_send_screenshot(uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_SCREENSHOT);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_async(uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD && status != GLDB_STATUS_STOPPED);
    gldb_protocol_send_code(lib_out, REQ_ASYNC);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_state_tree(uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_STATE_TREE);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_data_texture(uint32_t id, GLuint tex_id, GLenum target,
                            GLenum face, GLint level, GLenum format,
                            GLenum type)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_TEXTURE);
    gldb_protocol_send_code(lib_out, tex_id);
    gldb_protocol_send_code(lib_out, target);
    gldb_protocol_send_code(lib_out, face);
    gldb_protocol_send_code(lib_out, level);
    gldb_protocol_send_code(lib_out, format);
    gldb_protocol_send_code(lib_out, type);
}

void gldb_send_data_framebuffer(uint32_t id, GLuint fbo_id, GLenum target,
                                GLenum buffer, GLenum format, GLenum type)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_FRAMEBUFFER);
    gldb_protocol_send_code(lib_out, fbo_id);
    gldb_protocol_send_code(lib_out, target);
    gldb_protocol_send_code(lib_out, buffer);
    gldb_protocol_send_code(lib_out, format);
    gldb_protocol_send_code(lib_out, type);
}

void gldb_send_data_shader(uint32_t id, GLuint shader_id, GLenum target)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_SHADER);
    gldb_protocol_send_code(lib_out, shader_id);
    gldb_protocol_send_code(lib_out, target);
}

bool gldb_get_break_error(void)
{
    return break_on_error;
}

void gldb_set_break_error(uint32_t id, bool brk)
{
    break_on_error = brk;
    if (status != GLDB_STATUS_DEAD)
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK_ERROR);
        gldb_protocol_send_code(lib_out, id);
        gldb_protocol_send_code(lib_out, brk ? 1 : 0);
    }
}

void gldb_set_break(uint32_t id, const char *function, bool brk)
{
    bugle_hash_set(&break_on, function, brk ? "1" : "0");
    if (status != GLDB_STATUS_DEAD)
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK);
        gldb_protocol_send_code(lib_out, id);
        gldb_protocol_send_string(lib_out, function);
        gldb_protocol_send_code(lib_out, brk ? 1 : 0);
    }
}

void gldb_notify_child_dead(void)
{
    set_status(GLDB_STATUS_DEAD);
}

const char *gldb_get_chain(void)
{
    return filter_chain;
}

void gldb_set_chain(const char *chain)
{
    if (filter_chain) free(filter_chain);
    if (chain) filter_chain = bugle_strdup(chain);
    else chain = NULL;
}

gldb_status gldb_get_status(void)
{
    return status;
}

pid_t gldb_get_child_pid(void)
{
    return child_pid;
}

const char *gldb_get_program(void)
{
    return prog;
}

int gldb_get_in_pipe(void)
{
    return lib_in;
}

int gldb_get_out_pipe(void)
{
    return lib_out;
}

void gldb_initialise(int argc, char * const *argv)
{
    int i;

    prog = argv[1];
    prog_argv = bugle_malloc(sizeof(char *) * argc);
    prog_argv[argc - 1] = NULL;
    for (i = 1; i < argc; i++)
        prog_argv[i - 1] = argv[i];

    bugle_initialise_hashing();
    bugle_hash_init(&break_on, false);
}

void gldb_shutdown(void)
{
    /* If the process is running, we tell it to stop. It will then
     * find QUIT on the command buffer and quit.
     */
    if (gldb_get_status() == GLDB_STATUS_RUNNING)
        gldb_send_async(0);
    if (gldb_get_status() != GLDB_STATUS_DEAD)
        gldb_send_quit(0);
    bugle_hash_clear(&break_on);
    free(prog_argv);
}
