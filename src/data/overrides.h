#ifndef OVERRIDES_H
#define OVERRIDES_H

/* Mesa 3.4 gets this wrong */
#if defined(GL_ALL_CLIENT_ATTRIB_BITS) && !defined(GL_CLIENT_ALL_ATTRIB_BITS)
# define GL_CLIENT_ALL_ATTRIB_BITS GL_ALL_CLIENT_ATTRIB_BITS
#endif

typedef const GLfloat (*GLfloatmatrix)[4][4];
typedef const GLdouble (*GLdoublematrix)[4][4];
typedef const GLubyte (*GLstipple)[32][4];
typedef GLenum GLerror;         /* since GL_NO_ERROR conflicts with other tokens */
typedef GLenum GLalternateenum; /* handles general conflicts */
typedef GLenum GLcomponentsenum; /* reinterpret 1, 2, 3, 4 */

typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    GLvoid *pointer;

    GLboolean enabled;
    GLuint binding;
} GLarray;

#endif
