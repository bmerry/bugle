/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009-2010  Bruce Merry
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
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include "gldb/gldb-common.h"
#include <bugle/bool.h>
#include <bugle/linkedlist.h>
#include <bugle/hashtable.h>
#include <bugle/porting.h>
#include <bugle/string.h>
#include <bugle/memory.h>
#include <bugle/io.h>
#include <budgie/reflect.h>
#include "budgielib/defines.h"
#include "common/protocol.h"
#include "platform/types.h"

static bugle_io_reader *lib_in = NULL;
static bugle_io_writer *lib_out = NULL;
/* Started is set to BUGLE_TRUE only after receiving RESP_RUNNING. When
 * we are in the state (running && !started), non-break responses are
 * handled but do not cause a new line to be read.
 */
static gldb_status status = GLDB_STATUS_DEAD;
static bugle_pid_t child_pid = -1;

static char *prog_settings[GLDB_PROGRAM_SETTING_COUNT];
static gldb_program_type prog_type;

static gldb_state *state_root = NULL;

static bugle_bool break_on_event[REQ_EVENT_COUNT];
static hash_table break_on;

static void state_destroy(gldb_state *s)
{
    if (s == NULL) return;
    bugle_list_clear(&s->children);
    bugle_free(s->name);
    bugle_free(s->data);
    bugle_free(s);
}

void gldb_program_clear(void)
{
    int i;

    for (i = 0; i < GLDB_PROGRAM_SETTING_COUNT; i++)
    {
        bugle_free(prog_settings[i]);
        prog_settings[i] = NULL;
    }
    prog_type = GLDB_PROGRAM_TYPE_LOCAL;
}

bugle_bool gldb_program_type_has_setting(gldb_program_type type, gldb_program_setting setting)
{
    static const bugle_bool ans[GLDB_PROGRAM_TYPE_COUNT][GLDB_PROGRAM_SETTING_COUNT] =
    {
        /* GLDB_PROGRAM_TYPE_LOCAL */
        { BUGLE_TRUE, BUGLE_TRUE, BUGLE_TRUE, BUGLE_FALSE, BUGLE_FALSE },
        /* GLDB_PROGRAM_TYPE_GDB */
        { BUGLE_TRUE, BUGLE_TRUE, BUGLE_TRUE, BUGLE_FALSE, BUGLE_FALSE },
        /* GLDB_PROGRAM_TYPE_SSH */
        { BUGLE_TRUE, BUGLE_TRUE, BUGLE_TRUE, BUGLE_TRUE, BUGLE_FALSE },
        /* GLDB_PROGRAM_TYPE_TCP */
        { BUGLE_FALSE, BUGLE_FALSE, BUGLE_FALSE, BUGLE_TRUE, BUGLE_TRUE }
    };

    assert(type >= 0 && type < GLDB_PROGRAM_TYPE_COUNT);
    assert(setting >= 0 && setting < GLDB_PROGRAM_SETTING_COUNT);
    return ans[type][setting];
}

void gldb_program_set_setting(gldb_program_setting setting, const char *value)
{
    assert(setting < GLDB_PROGRAM_SETTING_COUNT);
    bugle_free(prog_settings[setting]);
    if (value && !*value) value = NULL;
    prog_settings[setting] = value ? bugle_strdup(value) : NULL;
}

void gldb_program_set_type(gldb_program_type type)
{
    prog_type = type;
}

const char *gldb_program_get_setting(gldb_program_setting setting)
{
    assert(setting < GLDB_PROGRAM_SETTING_COUNT);
    return prog_settings[setting];
}

gldb_program_type gldb_program_get_type(void)
{
    return prog_type;
}

const char *gldb_program_validate(void)
{
    const char *setting;

    switch (gldb_program_get_type())
    {
    case GLDB_PROGRAM_TYPE_LOCAL:
    case GLDB_PROGRAM_TYPE_GDB:
        setting = gldb_program_get_setting(GLDB_PROGRAM_SETTING_COMMAND);
        if (setting == NULL || setting[0] == '\0')
            return "Command not set";
        break;
    case GLDB_PROGRAM_TYPE_SSH:
        setting = gldb_program_get_setting(GLDB_PROGRAM_SETTING_COMMAND);
        if (setting == NULL || setting[0] == '\0')
            return "Command not set";
        setting = gldb_program_get_setting(GLDB_PROGRAM_SETTING_HOST);
        if (setting == NULL || setting[0] == '\0')
            return "Host not set";
        break;
    case GLDB_PROGRAM_TYPE_TCP:
        setting = gldb_program_get_setting(GLDB_PROGRAM_SETTING_HOST);
        if (setting == NULL || setting[0] == '\0')
            return "Host not set";
        break;
        setting = gldb_program_get_setting(GLDB_PROGRAM_SETTING_PORT);
        if (setting == NULL || setting[0] == '\0')
            return "Port not set";
        break;
    default:
        assert(0); /* Should never be reached */
    }
    return NULL;    /* success */
}

#if BUGLE_PLATFORM_MSVCRT || BUGLE_PLATFORM_MINGW
# include <windows.h>
# include <io.h>
# include <fcntl.h>
# include <process.h>

static void setenv_printf(const char *fmt, ...)
{
    va_list ap;
    char *var;

    va_start(ap, fmt);
    var = bugle_vasprintf(fmt, ap);
    va_end(ap);
    _putenv(var);
    bugle_free(var);
}

/* Produces an argv array by splitting the fields of the command.
 * To clean up, bugle_free(argv[0]), then bugle_free(argv).
 *
 * The field is split on individual spaces, not blocks of it. _spawnvp will
 * concatenate the arguments with spaces between, so this gives the original
 * command line.
 */
static char **make_argv(const char *cmd)
{
    char **argv;
    char *buffer, *i;
    int argc = 0;

    buffer = bugle_strdup(cmd);
    /* First pass to count arguments */
    i = buffer;
    while (*i == ' ') i++;  /* Skip leading whitespace */
    while (*i)
    {
        argc++;
        while (*i && *i != ' ')
            i++;      /* walk over argument */
        if (*i) i++;  /* skip one whitespace */
    }

    if (argc == 0)
    {
        /* empty argument list. We can't pass that back, because then the
         * caller would have no command to run.
         */
        argv = BUGLE_NMALLOC(2, char *);
        argv[0] = buffer;
        argv[1] = NULL;
        return argv;
    }

    argv = BUGLE_NMALLOC(argc + 1, char *);
    argc = 0;
    i = buffer;
    while (*i == ' ') i++;  /* Skip leading whitespace */
    while (*i)
    {
        argv[argc++] = i;
        while (*i && *i != ' ')
            i++;              /* walk over argument */
        if (*i) *i++ = '\0';  /* replace one space */
    }
    argv[argc] = NULL;

    return argv;
}

/* Spawns off the program, and returns the pid */
static bugle_pid_t execute(void (*child_init)(void))
{
    bugle_pid_t pid;
    /* in is child->parent
     * out is parent->child
     */
    HANDLE in_pipe[2], out_pipe[2];
    const char *command, *chain;
    char **argv;

    /* child_init not actually possible on win32 */
    assert(child_init == NULL);
    /* FIXME: add support for remote modes one day */
    switch (prog_type)
    {
    case GLDB_PROGRAM_TYPE_LOCAL:
        break;
    case GLDB_PROGRAM_TYPE_GDB:
        gldb_error("GDB mode not yet supported on Windows");
        break;
    case GLDB_PROGRAM_TYPE_SSH:
        gldb_error("SSH mode not yet supported on Windows");
        return -1;
    case GLDB_PROGRAM_TYPE_TCP:
        gldb_error("TCP/IP mode not yet supported on Windows");
        return -1;
    default:
        gldb_error("This mode is not yet supported on Windows");
        return -1;
    }

    command = prog_settings[GLDB_PROGRAM_SETTING_COMMAND];
    if (!command) command = "/bin/true";
    chain = prog_settings[GLDB_PROGRAM_SETTING_CHAIN];

    if (!CreatePipe(&in_pipe[0], &in_pipe[1], NULL, 0))
    {
        gldb_error("Failed to create pipes");
        return -1;
    }
    if (!CreatePipe(&out_pipe[0], &out_pipe[1], NULL, 0))
    {
        gldb_error("Failed to create pipes");
        CloseHandle(in_pipe[0]);
        CloseHandle(in_pipe[1]);
        return -1;
    }
    /* Allow the child ends to be inheritable */
    if (!SetHandleInformation(out_pipe[0], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)
        || !SetHandleInformation(in_pipe[1], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
    {
        gldb_error("Failed to make handles inheritable");
        CloseHandle(in_pipe[0]);
        CloseHandle(in_pipe[1]);
        CloseHandle(out_pipe[0]);
        CloseHandle(out_pipe[1]);
        return -1;
    }

    setenv_printf("BUGLE_DEBUGGER=handle");
    setenv_printf("BUGLE_DEBUGGER_HANDLE_IN=%#I64x", (unsigned __int64) (uintptr_t) out_pipe[0]);
    setenv_printf("BUGLE_DEBUGGER_HANDLE_OUT=%#I64x", (unsigned __int64) (uintptr_t) in_pipe[1]);
    setenv_printf("BUGLE_CHAIN=%s", chain ? chain : "");

    argv = make_argv(command);
    pid = _spawnvp(_P_NOWAIT, argv[0], (const char * const *) argv);
    bugle_free(argv[0]);
    bugle_free(argv);
    CloseHandle(out_pipe[0]);
    CloseHandle(in_pipe[1]);
    if (pid == -1)
    {
        gldb_error("failed to execute program");
        CloseHandle(in_pipe[0]);
        CloseHandle(out_pipe[1]);
        return -1;
    }

    lib_in = bugle_io_reader_handle_new(in_pipe[0], BUGLE_FALSE);
    lib_out = bugle_io_writer_handle_new(out_pipe[1], BUGLE_TRUE);
    return pid;
}

#else /* !BUGLE_PLATFORM_MSVCRT */
# include <unistd.h>
# include <sys/wait.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <netdb.h>

/* Spawns off the program, and returns the pid
 * Returns: -1 on failure
 *           pid on success
 */
static bugle_pid_t execute(void (*child_init)(void))
{
    bugle_pid_t pid;
    /* in/out refers to our view, not child view */
    int in_pipe[2], out_pipe[2];
    char *prog_argv[20];
    char *command, *chain, *display, *host, *port;

    if (prog_type == GLDB_PROGRAM_TYPE_TCP)
    {
        /* No exec() call, just open a connection */
        struct addrinfo hints;
        struct addrinfo *ai;
        int status;
        int sock, write_sock;

        host = prog_settings[GLDB_PROGRAM_SETTING_HOST];
        port = prog_settings[GLDB_PROGRAM_SETTING_PORT];

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
        status = getaddrinfo(host, port, &hints, &ai);
        if (status != 0 || ai == NULL)
        {
            gldb_error("Failed to resolve %s:%s: %s\n", 
                       host ? host : "",
                       port ? port : "",
                       gai_strerror(status));
            return -1;
        }

        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock == -1)
        {
            freeaddrinfo(ai);
            gldb_perror("socket");
            return -1;
        }
        if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1)
        {
            freeaddrinfo(ai);
            close(sock);
            gldb_perror("connect failed");
            return -1;
        }
        freeaddrinfo(ai);

        write_sock = dup(sock);
        if (write_sock == -1)
        {
            close(sock);
            gldb_perror("socket duplication failed");
            return -1;
        }

        lib_in = bugle_io_reader_fd_new(sock);
        lib_out = bugle_io_writer_fd_new(write_sock);
        return 0;
    }

    gldb_safe_syscall(pipe(in_pipe), "pipe");
    gldb_safe_syscall(pipe(out_pipe), "pipe");
    switch ((pid = fork()))
    {
    case -1:
        gldb_perror("fork failed");
        return -1;
    case 0: /* Child */
        if (child_init != NULL)
            (*child_init)();

        command = prog_settings[GLDB_PROGRAM_SETTING_COMMAND];
        if (!command) command = "/bin/true";
        display = prog_settings[GLDB_PROGRAM_SETTING_DISPLAY];
        chain = prog_settings[GLDB_PROGRAM_SETTING_CHAIN];
        host = prog_settings[GLDB_PROGRAM_SETTING_HOST];
        switch (prog_type)
        {
        case GLDB_PROGRAM_TYPE_LOCAL:
            prog_argv[0] = "sh";
            prog_argv[1] = "-c";
            prog_argv[2] = bugle_asprintf("%s%s BUGLE_CHAIN=%s LD_PRELOAD=libbugle.so BUGLE_DEBUGGER=fd BUGLE_DEBUGGER_FD_IN=%d BUGLE_DEBUGGER_FD_OUT=%d exec %s",
                                          display ? "DISPLAY=" : "", display ? display : "",
                                          chain ? chain : "",
                                          out_pipe[0], in_pipe[1], command);
            prog_argv[3] = NULL;
            break;
        case GLDB_PROGRAM_TYPE_GDB:
            {
                int p = 0;
                prog_argv[p++] = "xterm";
                prog_argv[p++] = "-e";
                prog_argv[p++] = "gdb";
                if (display)
                {
                    prog_argv[p++] = "-ex";
                    prog_argv[p++] = bugle_asprintf("set env DISPLAY %s", display);
                }
                if (chain)
                {
                    prog_argv[p++] = "-ex";
                    prog_argv[p++] = bugle_asprintf("set env BUGLE_CHAIN=%s", chain);
                }
                prog_argv[p++] = "-ex";
                prog_argv[p++] = "set env LD_PRELOAD=libbugle.so";
                prog_argv[p++] = "-ex";
                prog_argv[p++] = "set env BUGLE_DEBUGGER=fd";
                prog_argv[p++] = "-ex";
                prog_argv[p++] = bugle_asprintf("set env BUGLE_DEBUGGER_FD_IN=%d", out_pipe[0]);
                prog_argv[p++] = "-ex";
                prog_argv[p++] = bugle_asprintf("set env BUGLE_DEBUGGER_FD_OUT=%d", in_pipe[1]);
                prog_argv[p++] = command;
                prog_argv[p++] = NULL;
            }
            break;
        case GLDB_PROGRAM_TYPE_SSH:
            /* Insert the environment variables into the command. */
            prog_argv[0] = "ssh";
            prog_argv[1] = host ? host : "localhost";
            prog_argv[2] = bugle_asprintf("%s%s BUGLE_CHAIN=%s LD_PRELOAD=libbugle.so BUGLE_DEBUGGER=fd BUGLE_DEBUGGER_FD_IN=3 BUGLE_DEBUGGER_FD_OUT=4 %s 3<&0 4>&1 </dev/null 1>&2",
                                          display ? "DISPLAY=" : "", display ? display : "",
                                          chain ? chain : "",
                                          command);
            prog_argv[3] = NULL;
            dup2(in_pipe[1], 1);
            dup2(out_pipe[0], 0);
            break;
        default:;
        }

        close(in_pipe[0]);
        close(out_pipe[1]);
        execvp(prog_argv[0], prog_argv);
        perror("failed to execute program");
        exit(1);
    default: /* Parent */
        lib_in = bugle_io_reader_fd_new(in_pipe[0]);
        lib_out = bugle_io_writer_fd_new(out_pipe[1]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        switch (prog_type)
        {
        case GLDB_PROGRAM_TYPE_LOCAL:
            return pid;
        default:
            return 0;
        }
    }
}
#endif

static void set_status(gldb_status s)
{
    if (status == s) return;
    status = s;
    switch (status)
    {
    case GLDB_STATUS_RUNNING:
    case GLDB_STATUS_STOPPED:
        state_destroy(state_root);
        state_root = NULL;
        break;
    case GLDB_STATUS_STARTED:
        break;
    case GLDB_STATUS_DEAD:
        if (lib_in)
            bugle_io_reader_close(lib_in);
        if (lib_out)
            bugle_io_writer_close(lib_out);
        lib_in = NULL;
        lib_out = NULL;
        child_pid = 0;
        break;
    }
}

const gldb_state *gldb_state_get_root(void)
{
    return state_root;
}

static gldb_response *gldb_get_response_ans(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_ans *r;

    r = BUGLE_MALLOC(gldb_response_ans);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_code(lib_in, &r->value);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_break(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_break *r;

    r = BUGLE_MALLOC(gldb_response_break);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->call);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_break_event(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_break_event *r;

    r = BUGLE_MALLOC(gldb_response_break_event);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->call);
    gldb_protocol_recv_string(lib_in, &r->event);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_state(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_state *r;

    r = BUGLE_MALLOC(gldb_response_state);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_string(lib_in, &r->state);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_error(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_error *r;

    r = BUGLE_MALLOC(gldb_response_error);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_code(lib_in, &r->error_code);
    gldb_protocol_recv_string(lib_in, &r->error);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_running(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_running *r;
    bugle_uint64_t pid;

    r = BUGLE_MALLOC(gldb_response_running);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_code64(lib_in, &pid);
    r->pid = pid;
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_screenshot(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_screenshot *r;

    r = BUGLE_MALLOC(gldb_response_screenshot);
    r->code = code;
    r->id = id;
    gldb_protocol_recv_binary_string(lib_in, &r->length, &r->data);
    return (gldb_response *) r;
}

/* Recursively retrieves a state tree */
static gldb_state *state_get(void)
{
    gldb_state *s, *child;
    bugle_uint32_t resp, id;
    bugle_uint32_t numeric_name, enum_name;
    bugle_int32_t length;
    char *data;
    bugle_uint32_t data_len;
    char *type_name = NULL;

    s = BUGLE_MALLOC(gldb_state);
    bugle_list_init(&s->children, (void (*)(void *)) state_destroy);
    gldb_protocol_recv_string(lib_in, &s->name);
    gldb_protocol_recv_code(lib_in, &numeric_name);
    gldb_protocol_recv_code(lib_in, &enum_name);
    gldb_protocol_recv_string(lib_in, &type_name);
    gldb_protocol_recv_code(lib_in, (bugle_uint32_t *) &length);
    gldb_protocol_recv_binary_string(lib_in, &data_len, &data);
    s->numeric_name = numeric_name;
    s->enum_name = enum_name;
    s->type = budgie_type_id(type_name); bugle_free(type_name);
    s->length = length;
    s->data = data;

    do
    {
        if (!gldb_protocol_recv_code(lib_in, &resp)
            || !gldb_protocol_recv_code(lib_in, &id))
        {
            fprintf(stderr, "Pipe closed unexpectedly\n");
            exit(1); /* FIXME: can this be handled better? */
        }
        switch (resp)
        {
        case RESP_STATE_NODE_BEGIN_RAW:
            child = state_get();
            bugle_list_append(&s->children, child);
            break;
        case RESP_STATE_NODE_END_RAW:
            break;
        default:
            fprintf(stderr, "Unexpected code %08lx in state tree\n",
                   (unsigned long) resp);
            exit(1);
        }
    } while (resp != RESP_STATE_NODE_END_RAW);

    return s;
}

static gldb_response *gldb_get_response_state_tree(bugle_uint32_t code, bugle_uint32_t id)
{
    gldb_response_state_tree *r;

    r = BUGLE_MALLOC(gldb_response_state_tree);
    r->code = code;
    r->id = id;
    r->root = state_get();
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_texture(bugle_uint32_t code, bugle_uint32_t id,
                                                     bugle_uint32_t subtype,
                                                     bugle_uint32_t length, char *data)
{
    gldb_response_data_texture *r;

    r = BUGLE_MALLOC(gldb_response_data_texture);
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

static gldb_response *gldb_get_response_data_framebuffer(bugle_uint32_t code, bugle_uint32_t id,
                                                         bugle_uint32_t subtype,
                                                         bugle_uint32_t length, char *data)
{
    gldb_response_data_framebuffer *r;

    r = BUGLE_MALLOC(gldb_response_data_framebuffer);
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    gldb_protocol_recv_code(lib_in, &r->width);
    gldb_protocol_recv_code(lib_in, &r->height);
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_shader(bugle_uint32_t code, bugle_uint32_t id,
                                                    bugle_uint32_t subtype,
                                                    bugle_uint32_t length, char *data)
{
    gldb_response_data_shader *r;

    r = BUGLE_MALLOC(gldb_response_data_shader);
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_info_log(bugle_uint32_t code, bugle_uint32_t id,
                                                      bugle_uint32_t subtype,
                                                      bugle_uint32_t length, char *data)
{
    gldb_response_data_info_log *r;

    r = BUGLE_MALLOC(gldb_response_data_info_log);
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data_buffer(bugle_uint32_t code, bugle_uint32_t id,
                                                    bugle_uint32_t subtype,
                                                    bugle_uint32_t length, char *data)
{
    gldb_response_data_buffer *r;

    r = BUGLE_MALLOC(gldb_response_data_buffer);
    r->code = code;
    r->id = id;
    r->subtype = subtype;
    r->length = length;
    r->data = data;
    return (gldb_response *) r;
}

static gldb_response *gldb_get_response_data(bugle_uint32_t code, bugle_uint32_t id)
{
    bugle_uint32_t subtype;
    bugle_uint32_t length;
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
    case REQ_DATA_INFO_LOG:
        return gldb_get_response_data_info_log(code, id, subtype, length, data);
    case REQ_DATA_BUFFER:
        return gldb_get_response_data_buffer(code, id, subtype, length, data);
    default:
        fprintf(stderr, "Unknown DATA subtype %lu\n", (unsigned long) subtype);
        exit(1);
    }
}

gldb_response *gldb_get_response(void)
{
    bugle_uint32_t code, id;

    if (!gldb_protocol_recv_code(lib_in, &code)
        || !gldb_protocol_recv_code(lib_in, &id))
        return NULL;

    switch (code)
    {
    case RESP_ANS: return gldb_get_response_ans(code, id);
    case RESP_BREAK: return gldb_get_response_break(code, id);
    case RESP_STOP: /* Obsolete alias of RESP_BREAK */
        return gldb_get_response_break(RESP_BREAK, id);
    case RESP_BREAK_EVENT: return gldb_get_response_break_event(code, id);
    case RESP_STATE: return gldb_get_response_state(code, id);
    case RESP_ERROR: return gldb_get_response_error(code, id);
    case RESP_RUNNING: return gldb_get_response_running(code, id);
    case RESP_SCREENSHOT: return gldb_get_response_screenshot(code, id);
    case RESP_STATE_NODE_BEGIN_RAW: return gldb_get_response_state_tree(code, id);
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
        bugle_free(((gldb_response_break *) r)->call);
        break;
    case RESP_BREAK_EVENT:
        bugle_free(((gldb_response_break_event *) r)->call);
        bugle_free(((gldb_response_break_event *) r)->event);
        break;
    case RESP_STATE:
        bugle_free(((gldb_response_state *) r)->state);
        break;
    case RESP_ERROR:
        bugle_free(((gldb_response_error *) r)->error);
        break;
    case RESP_SCREENSHOT:
        bugle_free(((gldb_response_screenshot *) r)->data);
        break;
    case RESP_STATE_NODE_BEGIN:
        state_destroy(((gldb_response_state_tree *) r)->root);
        break;
    case RESP_DATA:
        bugle_free(((gldb_response_data *) r)->data);
        break;
    }
    bugle_free(r);
}

void gldb_process_response(gldb_response *r)
{
    switch (r->code)
    {
    case RESP_BREAK:
    case RESP_BREAK_EVENT:
        set_status(GLDB_STATUS_STOPPED);
        break;
    case RESP_RUNNING:
        child_pid = ((gldb_response_running *) r)->pid;
        set_status(GLDB_STATUS_RUNNING);
        break;
    case RESP_STATE_NODE_BEGIN_RAW:
        {
            gldb_response_state_tree *resp = (gldb_response_state_tree *) r;
            state_destroy(state_root);
            state_root = resp->root;
            resp->root = NULL;  /* Prevent gldb_free_response from clearing it */
        }
        break;
    default:
        break;
    }
}

/* Only first n characters of name are considered. This simplifies
 * generate_commands.
 */
gldb_state *gldb_state_find(const gldb_state *root, const char *name, size_t n)
{
    const char *split;
    linked_list_node *i;
    gldb_state *child;
    bugle_bool found;

    if (n > strlen(name)) n = strlen(name);
    while (n > 0)
    {
        found = BUGLE_FALSE;
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
                found = BUGLE_TRUE;
                root = child;
                n -= split - name;
                name = split;
                break;
            }
        }
        if (!found) return NULL;
    }
    return (gldb_state *) root;
}

gldb_state *gldb_state_find_child_numeric(const gldb_state *parent, GLint name)
{
    gldb_state *child;
    linked_list_node *i;

    /* FIXME: build indices on the state */
    for (i = bugle_list_head(&parent->children); i; i = bugle_list_next(i))
    {
        child = (gldb_state *) bugle_list_data(i);
        if (child->numeric_name == name) return child;
    }
    return NULL;
}

gldb_state *gldb_state_find_child_enum(const gldb_state *parent, GLenum name)
{
    gldb_state *child;
    linked_list_node *i;

    /* FIXME: build indices on the state */
    for (i = bugle_list_head(&parent->children); i; i = bugle_list_next(i))
    {
        child = (gldb_state *) bugle_list_data(i);
        if (child->enum_name == name) return child;
    }
    return NULL;
}

char *gldb_state_string(const gldb_state *state)
{
    if (state->length == 0)
        return bugle_strdup("");
    if (state->length == -2)
        return bugle_strdup("<GL error>");
    if (state->type == TYPE_c || state->type == TYPE_Kc)
        return bugle_strdup((const char *) state->data);
    else
    {
        bugle_io_writer *writer;
        char *ans;

        writer = bugle_io_writer_mem_new(64);
        budgie_dump_any_type_extended(state->type, state->data, -1, state->length, NULL, writer);
        ans = bugle_io_writer_mem_get(writer);
        bugle_io_writer_close(writer);
        return ans;
    }
}

GLint gldb_state_GLint(const gldb_state *state)
{
    if (state->type == TYPE_5GLint || state->type == TYPE_6GLuint)
        return *(GLint *) state->data;
    else
    {
        GLint out;
        budgie_type_convert(&out, TYPE_5GLint, state->data, state->type, 1);
        return out;
    }
}

GLenum gldb_state_GLenum(const gldb_state *state)
{
    if (state->type == TYPE_6GLenum)
        return *(GLenum *) state->data;
    else
    {
        GLenum out;
        budgie_type_convert(&out, TYPE_6GLenum, state->data, state->type, 1);
        return out;
    }
}

GLboolean gldb_state_GLboolean(const gldb_state *state)
{
    if (state->type == TYPE_9GLboolean)
        return *(GLboolean *) state->data;
    else
    {
        GLboolean out;
        budgie_type_convert(&out, TYPE_9GLboolean, state->data, state->type, 1);
        return out;
    }
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

bugle_bool gldb_execute(void (*child_init)(void))
{
    child_pid = execute(child_init);
    if (child_pid == -1)
        return BUGLE_FALSE;
    else
        return BUGLE_TRUE;
}

bugle_bool gldb_run(bugle_uint32_t id)
{
    const hash_table_entry *h;
    bugle_uint32_t event;

    /* Send breakpoints */
    for (event = 0; event < REQ_EVENT_COUNT; event++)
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK_EVENT);
        gldb_protocol_send_code(lib_out, 0);
        gldb_protocol_send_code(lib_out, event);
        gldb_protocol_send_code(lib_out, break_on_event[event] ? 1 : 0);
    }
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
    return BUGLE_TRUE;
}

void gldb_send_continue(bugle_uint32_t id)
{
    assert(status == GLDB_STATUS_STOPPED);
    set_status(GLDB_STATUS_RUNNING);
    gldb_protocol_send_code(lib_out, REQ_CONT);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_step(bugle_uint32_t id)
{
    assert(status == GLDB_STATUS_STOPPED);
    set_status(GLDB_STATUS_RUNNING);
    gldb_protocol_send_code(lib_out, REQ_STEP);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_quit(bugle_uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_QUIT);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_enable_disable(bugle_uint32_t id, const char *filterset, bugle_bool enable)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, enable ? REQ_ACTIVATE_FILTERSET : REQ_DEACTIVATE_FILTERSET);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_string(lib_out, filterset);
}

void gldb_send_screenshot(bugle_uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_SCREENSHOT);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_async(bugle_uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD && status != GLDB_STATUS_STOPPED);
    gldb_protocol_send_code(lib_out, REQ_ASYNC);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_state_tree(bugle_uint32_t id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_STATE_TREE_RAW);
    gldb_protocol_send_code(lib_out, id);
}

void gldb_send_data_texture(bugle_uint32_t id, GLuint tex_id, GLenum target,
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

void gldb_send_data_framebuffer(bugle_uint32_t id, GLuint fbo_id,
                                GLenum buffer, GLenum format, GLenum type)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_FRAMEBUFFER);
    gldb_protocol_send_code(lib_out, fbo_id);
    gldb_protocol_send_code(lib_out, buffer);
    gldb_protocol_send_code(lib_out, format);
    gldb_protocol_send_code(lib_out, type);
}

void gldb_send_data_shader(bugle_uint32_t id, GLuint shader_id, GLenum target)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_SHADER);
    gldb_protocol_send_code(lib_out, shader_id);
    gldb_protocol_send_code(lib_out, target);
}

void gldb_send_data_info_log(bugle_uint32_t id, GLuint object_id, GLenum target)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_INFO_LOG);
    gldb_protocol_send_code(lib_out, object_id);
    gldb_protocol_send_code(lib_out, target);
}

void gldb_send_data_buffer(bugle_uint32_t id, GLuint object_id)
{
    assert(status != GLDB_STATUS_DEAD);
    gldb_protocol_send_code(lib_out, REQ_DATA);
    gldb_protocol_send_code(lib_out, id);
    gldb_protocol_send_code(lib_out, REQ_DATA_BUFFER);
    gldb_protocol_send_code(lib_out, object_id);
}

bugle_bool gldb_get_break_event(bugle_uint32_t event)
{
    assert(event < REQ_EVENT_COUNT);
    return break_on_event[event];
}

void gldb_set_break_event(bugle_uint32_t id, bugle_uint32_t event, bugle_bool brk)
{
    assert(event < REQ_EVENT_COUNT);
    break_on_event[event] = brk;
    if (status != GLDB_STATUS_DEAD)
    {
        gldb_protocol_send_code(lib_out, REQ_BREAK_EVENT);
        gldb_protocol_send_code(lib_out, id);
        gldb_protocol_send_code(lib_out, event);
        gldb_protocol_send_code(lib_out, brk ? 1 : 0);
    }
}

void gldb_set_break(bugle_uint32_t id, const char *function, bugle_bool brk)
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

gldb_status gldb_get_status(void)
{
    return status;
}

bugle_pid_t gldb_get_child_pid(void)
{
    return child_pid;
}

void gldb_initialise(int argc, const char * const *argv)
{
    int i;
    char *command, *ptr;
    size_t len = 1;

    for (i = 1; i < argc; i++)
        len += strlen(argv[i]) + 1;
    ptr = command = BUGLE_NMALLOC(len, char);
    for (i = 1; i < argc; i++)
    {
        len = strlen(argv[i]);
        memcpy(ptr, argv[i], len);
        ptr += len;
        *ptr++ = ' ';
    }
    if (ptr > command)
        ptr[-1] = '\0';
    else
        ptr[0] = '\0';
    gldb_program_set_setting(GLDB_PROGRAM_SETTING_COMMAND, command);
    gldb_program_set_setting(GLDB_PROGRAM_SETTING_HOST, "localhost");
    gldb_program_set_setting(GLDB_PROGRAM_SETTING_PORT, "9118");
    bugle_free(command);
    bugle_hash_init(&break_on, NULL);
    for (i = 0; i < (int) REQ_EVENT_COUNT; i++)
        break_on_event[i] = BUGLE_TRUE;
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

    gldb_program_clear();
}
