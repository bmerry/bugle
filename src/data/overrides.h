#ifndef OVERRIDES_H
#define OVERRIDES_H

/* Mesa 3.4 gets this wrong */
#if defined(GL_ALL_CLIENT_ATTRIB_BITS) && !defined(GL_CLIENT_ALL_ATTRIB_BITS)
# define GL_CLIENT_ALL_ATTRIB_BITS GL_ALL_CLIENT_ATTRIB_BITS
#endif

/* Array types */
typedef GLfloat GLfloatmatrix[4][4];
typedef GLdouble GLdoublematrix[4][4];
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

/* Pointers to the array types */
typedef const GLfloatmatrix *pGLfloatmatrix;
typedef const GLdoublematrix *pGLdoublematrix;
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

/* Other ways to interpret enums */
typedef GLenum GLerror;          /* since GL_NO_ERROR conflicts with other tokens */
typedef GLenum GLalternateenum;  /* handles general conflicts */
typedef GLenum GLcomponentsenum; /* reinterpret 1, 2, 3, 4 */

#endif
