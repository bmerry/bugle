/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2009-2010  Bruce Merry
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

/* Generates triangles using a variety of OpenGL functions, to test the
 * triangle counting code.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
/* Required to compile GLUT under MinGW */
#if defined(_WIN32) && !defined(_STDCALL_SUPPORTED)
# define _STDCALL_SUPPORTED
#endif
#include <GL/glut.h>
#include "test.h"

static GLfloat float_data[18] =
{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f
};

static void triangles_immediate(GLenum mode, int count)
{
    GLfloat f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLdouble d[4] = {0.0, 0.0, 0.0, 0.0};
    GLshort s[4] = {0, 0, 0, 0};

    glBegin(mode);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex2d(0.0, 0.0);
    glVertex4s(0, 0, 0, 0);

    glVertex2fv(f);
    glVertex3sv(s);
    glVertex4dv(d);
    glEnd();
    test_log_printf("logstats\\.triangles per frame: %d triangles/frame\n", count);
}

static void triangles_draw_arrays(GLenum mode, int count)
{
    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    glDrawArrays(mode, 0, 6);
    test_log_printf("logstats\\.triangles per frame: %d triangles/frame\n", count);
}

static void triangles_draw_elements(GLenum mode, int count)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    glDrawElements(mode, 6, GL_UNSIGNED_SHORT, indices);
    test_log_printf("logstats\\.triangles per frame: %d triangles/frame\n", count);
}

static void triangles_draw_range_elements(GLenum mode, int count)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    if (GLEW_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(mode, 0, 5, 6, GL_UNSIGNED_SHORT, indices);
        test_log_printf("logstats\\.triangles per frame: %d triangles/frame\n", count);
    }
    else
    {
        test_skipped("EXT_draw_range_elements required");
        test_log_printf("logstats\\.triangles per frame: 0 triangles/frame\n");
    }
}

static void triangles_draw_arrays_instanced(GLenum mode, int count)
{
    glVertexPointer(GL_FLOAT, 3, 0, float_data);
#ifdef GL_ARB_draw_instanced
    if (GLEW_ARB_draw_instanced)
    {
        glDrawArraysInstancedARB(mode, 0, 6, 3);
        test_log_printf("logstats\\.triangles per frame: %d triangles/frame\n", count * 3);
    }
    else
#endif
    {
        test_skipped("ARB_draw_instanced required");
        test_log_printf("logstats\\.triangles per frame: 0 triangles/frame\n");
    }
}

static void triangles_draw_elements_instanced(GLenum mode, int count)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
#ifdef GL_ARB_draw_instanced
    if (GLEW_ARB_draw_instanced)
    {
        glDrawElementsInstancedARB(mode, 6, GL_UNSIGNED_SHORT, indices, 3);
        test_log_printf("logstats\\.triangles per frame: %d triangles/frame\n", count * 3);
    }
    else
#endif
    {
        test_skipped("ARB_draw_instanced required");
        test_log_printf("logstats\\.triangles per frame: 0 triangles/frame\n");
    }
}

static void run(void (*func)(GLenum, int))
{
    func(GL_POINTS, 0);
    glutSwapBuffers();
    func(GL_TRIANGLES, 2);
    glutSwapBuffers();
    func(GL_QUADS, 2);
    glutSwapBuffers();
    func(GL_TRIANGLE_STRIP, 4);
    glutSwapBuffers();
    func(GL_POLYGON, 4);
    glutSwapBuffers();
}

static void triangles_test_immediate(void)
{
    run(triangles_immediate);
}

static void triangles_test_draw_arrays(void)
{
    run(triangles_draw_arrays);
}

static void triangles_test_draw_elements(void)
{
    run(triangles_draw_elements);
}

static void triangles_test_draw_range_elements(void)
{
    run(triangles_draw_range_elements);
}

static void triangles_test_draw_arrays_instanced(void)
{
    run(triangles_draw_arrays_instanced);
}

static void triangles_test_draw_elements_instanced(void)
{
    run(triangles_draw_elements_instanced);
}

void triangles_suite_register(void)
{
    /* glutSwapBuffers is used for setup since there is no triangle on the first frame */
    test_suite *ts = test_suite_new("triangles", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, glutSwapBuffers, NULL);
    test_suite_add_test(ts, "immediate", triangles_test_immediate);
    test_suite_add_test(ts, "draw_arrays", triangles_test_draw_arrays);
    test_suite_add_test(ts, "draw_elements", triangles_test_draw_elements);
    test_suite_add_test(ts, "draw_range_elements", triangles_test_draw_range_elements);
    test_suite_add_test(ts, "draw_arrays_instanced", triangles_test_draw_arrays_instanced);
    test_suite_add_test(ts, "draw_elements_instanced", triangles_test_draw_elements_instanced);
}
