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

#ifndef BUGLE_GLDB_GLDB_COMMON_H
#define BUGLE_GLDB_GLDB_COMMON_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include "common/bool.h"
#include "common/linkedlist.h"

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

typedef struct
{
    char *name;
    char *value;
    bugle_linked_list children;
} gldb_state;

/* Only first n characters of name are considered. This simplifies
 * generate_commands.
 */
gldb_state *gldb_state_find(gldb_state *root, const char *name, size_t n);
/* Updates the internal state tree if necessary, and returns the tree */
gldb_state *gldb_state_update();

/* Checks that the result of a system call is not -1, otherwise throws
 * an error.
 */
void gldb_safe_syscall(int r, const char *str);

void gldb_run(void (*child_init)(void));
void gldb_send_quit(void);
void gldb_send_continue(void);
void gldb_send_enable_disable(const char *filterset, bool enable);
void gldb_send_screenshot(void);
void gldb_send_async(void);
void gldb_set_break_error(bool brk);
void gldb_set_break(const char *function, bool brk);
void gldb_set_chain(const char *chain);

void gldb_info_stopped(void);
void gldb_info_running(void);
void gldb_info_child_terminated(void);

bool gldb_running(void);
bool gldb_started(void);
pid_t gldb_child_pid(void);
const char *gldb_program(void);
int gldb_in_pipe(void);

void gldb_initialise(int argc, char * const *argv);
void gldb_shutdown(void);

#endif /* !BUGLE_GLDB_GLDB_COMMON_H */
