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
typedef GLfloat (*GLvec2)[2];
typedef GLfloat (*GLvec3)[3];
typedef GLfloat (*GLvec4)[4];
typedef GLint (*GLivec2)[2];
typedef GLint (*GLivec3)[3];
typedef GLint (*GLivec4)[4];
typedef GLfloat (*GLmat2)[2][2];
typedef GLfloat (*GLmat3)[3][3];
typedef GLfloat (*GLmat4)[4][4];
typedef GLint (*GLimat2)[2][2];
typedef GLint (*GLimat3)[3][3];
typedef GLint (*GLimat4)[4][4];

typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    GLvoid *pointer;

    GLboolean enabled;
    GLuint binding;
} GLarray;

#endif
