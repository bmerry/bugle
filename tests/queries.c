/* Generates a variety of OpenGL queries, mainly for testing the logger */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>

/* Query a bunch of things that are actually enumerants */
void query_enums(void)
{
    GLint i;
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &i);
    glGetIntegerv(GL_DEPTH_FUNC, &i);
    glGetIntegerv(GL_DRAW_BUFFER, &i);
}

/* Query a bunch of things that are actually booleans */
void query_bools(void)
{
    GLint i;

    /* Enables */
    glGetIntegerv(GL_ALPHA_TEST, &i);
    glGetIntegerv(GL_LIGHTING, &i);

    /* True booleans */
    glGetIntegerv(GL_DOUBLEBUFFER, &i);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &i);
    glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &i);
}

/* Query a bunch of things that are actually arrays */
void query_multi(void)
{
    GLint i[16];
    GLdouble d[16];
    glGetIntegerv(GL_COLOR_WRITEMASK, i);
    glGetDoublev(GL_CURRENT_COLOR, d);
    glGetDoublev(GL_MODELVIEW_MATRIX, d);
}

void query_texparameter(void)
{
    GLint i;
    GLfloat f[4];

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &i);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, &i);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, f);
}

void query_texlevelparameter(void)
{
    GLint i;

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &i);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &i);
#ifdef GL_TEXTURE_COMPRESSED_ARB
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &i);
#endif
}

void query_strings(void)
{
    glGetString(GL_VENDOR);
    glGetString(GL_RENDERER);
    glGetString(GL_EXTENSIONS);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("query generator");
    query_enums();
    query_bools();
    query_multi();
    query_texparameter();
    query_texlevelparameter();
    query_strings();
    return 0;
}
