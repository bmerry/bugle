#ifndef BUGLE_GL_OVERRIDES_H
#define BUGLE_GL_OVERRIDES_H

#include <bugle/porting.h>

/* Mesa 3.4 gets this wrong */
#if defined(GL_ALL_CLIENT_ATTRIB_BITS) && !defined(GL_CLIENT_ALL_ATTRIB_BITS)
# define GL_CLIENT_ALL_ATTRIB_BITS GL_ALL_CLIENT_ATTRIB_BITS
#endif

/* Array types */
typedef GLfloat GLfloatmatrix[4][4];
#if BUGLE_GLTYPE_GL
typedef GLdouble GLdoublematrix[4][4];
#endif
typedef GLubyte GLpolygonstipple[32][4];

typedef GLfloat GLvec2[2];
typedef GLfloat GLvec3[3];
typedef GLfloat GLvec4[4];
typedef GLint GLivec2[2];
typedef GLint GLivec3[3];
typedef GLint GLivec4[4];
typedef GLfloat GLmat2[2][2];
typedef GLfloat GLmat3[3][3];
typedef GLfloat GLmat4[4][4];
typedef GLboolean GLbvec2[2];
typedef GLboolean GLbvec3[3];
typedef GLboolean GLbvec4[4];

/* New types in GLSL 1.20 */
typedef GLfloat GLmat2x3[2][3];
typedef GLfloat GLmat3x2[3][2];
typedef GLfloat GLmat2x4[2][4];
typedef GLfloat GLmat4x2[4][2];
typedef GLfloat GLmat3x4[3][4];
typedef GLfloat GLmat4x3[4][3];

/* Pointers to the array types */
typedef const GLfloatmatrix *pGLfloatmatrix;
#if BUGLE_GLTYPE_GL
typedef const GLdoublematrix *pGLdoublematrix;
#endif
typedef const GLpolygonstipple *pGLpolygonstipple;

typedef GLvec2 *pGLvec2;
typedef GLvec3 *pGLvec3;
typedef GLvec4 *pGLvec4;
typedef GLivec2 *pGLivec2;
typedef GLivec3 *pGLivec3;
typedef GLivec4 *pGLivec4;
typedef GLmat2 *pGLmat2;
typedef GLmat3 *pGLmat3;
typedef GLmat4 *pGLmat4;
typedef GLbvec2 *pGLbvec2;
typedef GLbvec3 *pGLbvec3;
typedef GLbvec4 *pGLbvec4;

typedef GLmat2x3 *pGLmat2x3;
typedef GLmat3x2 *pGLmat3x2;
typedef GLmat2x4 *pGLmat2x4;
typedef GLmat4x2 *pGLmat4x2;
typedef GLmat3x4 *pGLmat3x4;
typedef GLmat4x3 *pGLmat4x3;

/* Pointer types that don't get detected automatically in all cases. */
typedef GLubyte *pGLubyte;
typedef GLushort *pGLushort;

/* Other ways to interpret enums. Enums have up to 5 different interpretations,
 * with the most serious aliasing being that of 0 and 1. */
typedef GLenum GLerror;          /* 0 = GL_NO_ERROR */
typedef GLenum GLblendenum;      /* 0 = GL_ZERO, 1 = GL_ONE */
typedef GLenum GLprimitiveenum;  /* 0 = GL_POINTS, 1 = GL_LINES */
typedef GLenum GLcomponentsenum; /* reinterpret 1, 2, 3, 4 as numerical */

/* Values passed to and from GL_NV_transform_feedback to specify an
 * attribute for feedback.
 */
typedef struct
{
    GLint attribute;
    GLint components;
    GLint index;
} GLxfbattrib;
typedef GLxfbattrib *pGLxfbattrib;
typedef const GLxfbattrib *pkGLxfbattrib;

#endif /* BUGLE_GL_OVERRIDES_H */
