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

#ifndef BUGLE_GLDB_GLDB_COMMON_H
#define BUGLE_GLDB_GLDB_COMMON_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
/* Nasty hack to make sure we get desktop GL even when other headers are
 * trying to pull in the target version (otherwise we have problems trying to
 * include glew.h if it's used in the GUI). Note that this means that
 * gldb-common.h must always preceed these other headers.
 */
#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#endif
#include <GL/gl.h>
#define BUGLE_GL_GLHEADERS_H
#define BUDGIE_TYPES2_H

#include <bugle/bool.h>
#include <bugle/linkedlist.h>
#include <bugle/attributes.h>
#include <bugle/gl/glheaders.h>
#include <budgie/basictypes.h>
#include "platform/types.h"
#include "platform/io.h"

/* Callback functions provided by the UI */
void gldb_error(const char *fmt, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2);
void gldb_perror(const char *msg);

/* There are four possible statuses:
 * - dead: the program has not been started or has exited
 * - started: the program is busy initialising and accepting our
 *   breakpoints
 * - running: we have received RESP_RUNNING
 * - stopped: the program is stopped on a breakpoint/error/interrupt
 */
typedef enum
{
    GLDB_STATUS_DEAD,
    GLDB_STATUS_STARTED,
    GLDB_STATUS_RUNNING,
    GLDB_STATUS_STOPPED
} gldb_status;

typedef enum
{
    GLDB_PROGRAM_SETTING_COMMAND = 0,
    GLDB_PROGRAM_SETTING_CHAIN,
    GLDB_PROGRAM_SETTING_DISPLAY,
    GLDB_PROGRAM_SETTING_HOST,
    GLDB_PROGRAM_SETTING_PORT,
    GLDB_PROGRAM_SETTING_COUNT
} gldb_program_setting;

typedef enum
{
    GLDB_PROGRAM_TYPE_LOCAL = 0,
    GLDB_PROGRAM_TYPE_SSH,
    GLDB_PROGRAM_TYPE_TCP,
    GLDB_PROGRAM_TYPE_COUNT
} gldb_program_type;

typedef struct
{
    char *name;
    GLint numeric_name;
    GLenum enum_name;
    budgie_type type;
    int length;
    void *data;
    linked_list children;
} gldb_state;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t value;
} gldb_response_ans;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    char *call;
} gldb_response_break;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    char *call;
    char *event;
} gldb_response_break_event;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    char *call;
} gldb_response_stop;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    char *state;
} gldb_response_state;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t error_code;
    char *error;
} gldb_response_error;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
} gldb_response_running;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    char *data;
    bugle_uint32_t length;
} gldb_response_screenshot;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    gldb_state *root;
} gldb_response_state_tree;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t subtype;
    char *data;
    bugle_uint32_t length;
    bugle_uint32_t width;
    bugle_uint32_t height;
    bugle_uint32_t depth;
} gldb_response_data_texture;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t subtype;
    char *data;
    bugle_uint32_t length;
    bugle_uint32_t width;
    bugle_uint32_t height;
} gldb_response_data_framebuffer;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t subtype;
    char *data;
    bugle_uint32_t length;
} gldb_response_data_shader;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t subtype;
    char *data;
    bugle_uint32_t length;
} gldb_response_data_info_log;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t subtype;
    char *data;
    bugle_uint32_t length;
} gldb_response_data_buffer;

typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
    bugle_uint32_t subtype;
    char *data;
    bugle_uint32_t length;
} gldb_response_data; /* Generic form of gldb_response_data_* */

/* Generic type for responses. Always instantiated via one of the above. */
typedef struct
{
    bugle_uint32_t code;
    bugle_uint32_t id;
} gldb_response;

/* Only first n characters of name are considered. This simplifies
 * generate_commands.
 */
gldb_state *gldb_state_find(const gldb_state *root, const char *name, size_t n);
/* Finds the immediate child with the given numeric/enum name, or NULL */
gldb_state *gldb_state_find_child_numeric(const gldb_state *parent, GLint name);
gldb_state *gldb_state_find_child_enum(const gldb_state *parent, GLenum name);
/* Converts a state to a string representation, which the caller must free */
char *gldb_state_string(const gldb_state *state);
GLint gldb_state_GLint(const gldb_state *state);
GLenum gldb_state_GLenum(const gldb_state *state);
GLboolean gldb_state_GLboolean(const gldb_state *state);

/* Retrieves the root of the state cache. Returns NULL if the cache is
 * invalid (use gldb_send_state_tree to refresh it asynchronously).
 */
const gldb_state *gldb_state_get_root(void);

/* Checks that the result of a system call is not -1, otherwise throws
 * an error.
 */
void gldb_safe_syscall(int r, const char *str);

/* Spawns the child process */
bugle_bool gldb_execute(void (*child_init)(void));
/* Sends initial breakpoints and similar data */
bugle_bool gldb_run(bugle_uint32_t id);

void gldb_send_quit(bugle_uint32_t id);
void gldb_send_continue(bugle_uint32_t id);
void gldb_send_step(bugle_uint32_t id);
void gldb_send_enable_disable(bugle_uint32_t id, const char *filterset, bugle_bool enable);
void gldb_send_screenshot(bugle_uint32_t id);
void gldb_send_async(bugle_uint32_t id);
void gldb_send_state_tree(bugle_uint32_t id);
void gldb_send_data_texture(bugle_uint32_t id, GLuint tex_id, GLenum target,
                            GLenum face, GLint level, GLenum format,
                            GLenum type);
void gldb_send_data_framebuffer(bugle_uint32_t id, GLuint fbo_id,
                                GLenum buffer, GLenum format, GLenum type);
void gldb_send_data_shader(bugle_uint32_t id, GLuint shader_id, GLenum target);
void gldb_send_data_info_log(bugle_uint32_t id, GLuint object_id, GLenum target);
void gldb_send_data_buffer(bugle_uint32_t id, GLuint object_id);
void gldb_set_break_event(bugle_uint32_t id, bugle_uint32_t event, bugle_bool brk);
void gldb_set_break(bugle_uint32_t id, const char *function, bugle_bool brk);

const char *gldb_get_chain(void);
void gldb_set_chain(const char *chain);
void gldb_notify_child_dead(void);

gldb_response *gldb_get_response(void);
void gldb_free_response(gldb_response *response);

/* Update internal flags based on the response. This is separate from
 * gldb_get_response since it should be run in the same thread that queries
 * those states, rather than in a thread that purely calls gldb_get_response.
 *
 * The specific states that are updated as a result:
 * - the program status
 * - the state cache
 * The state tree is removed from state responses so that the response may
 * be freed without breaking the cache.
 */
void gldb_process_response(gldb_response *response);

bugle_bool gldb_get_break_event(bugle_uint32_t event);
gldb_status gldb_get_status(void);
bugle_pid_t gldb_get_child_pid(void);

void gldb_program_clear(void);
void gldb_program_set_setting(gldb_program_setting setting, const char *value);
void gldb_program_set_type(gldb_program_type type);
bugle_bool gldb_program_type_has_setting(gldb_program_type type, gldb_program_setting setting);
const char *gldb_program_get_setting(gldb_program_setting setting);
gldb_program_type gldb_program_get_type(void);

/* Validates current settings, and returns an error message if they
 * are invalid. Otherwise, returns NULL.
 */
const char * gldb_program_validate(void);

void gldb_initialise(int argc, const char * const *argv);
void gldb_shutdown(void);

#endif /* !BUGLE_GLDB_GLDB_COMMON_H */
