/* Sets a variety of OpenGL state, mainly to test the logger */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>

void set_enables()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_TEXTURE_1D);
    glEnable(GL_TEXTURE_2D);
#ifdef GL_EXT_texture3D
    glEnable(GL_TEXTURE_3D_EXT);
#endif
#ifdef GL_ARB_texture_cube_map
    glEnable(GL_TEXTURE_CUBE_MAP_ARB);
#endif
    glEnable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_BLEND);
}

void set_client_state()
{
    glEnable(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_INT, 0, NULL);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStoref(GL_PACK_SWAP_BYTES, GL_FALSE);
}

void set_texture_state()
{
    GLuint id;
    GLint arg;

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
#ifdef GL_SGIS_generate_mipmap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif
    arg = GL_LINEAR;
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &arg);
    glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    glDeleteTextures(1, &id);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("state generator");
    set_enables();
    set_client_state();
    set_texture_state();
    return 0;
}
