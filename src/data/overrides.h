#ifndef OVERRIDES_H
#define OVERRIDES_H

typedef const GLfloat (*GLfloatmatrix)[4][4];
typedef const GLdouble (*GLdoublematrix)[4][4];
typedef const GLubyte (*GLstipple)[32][4];
typedef GLenum GLerror; // since GL_NO_ERROR conflicts with other tokens
typedef GLenum GLalternateenum; // handles general conflicts

typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    GLvoid *pointer;

    GLboolean enabled;
    GLuint binding;
} GLarray;

#endif
