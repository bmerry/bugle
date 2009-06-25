#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/glut.h>

static void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(2.0, 2.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    glutSolidCube(1.0);
    glutSwapBuffers();
}

static void idle(void)
{
    glutPostRedisplay();
}

static void reshape(int w, int h)
{
    glMatrixMode(GL_PROJECTION);
    gluPerspective(90.0, (double) w / (double) h, 0.01, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, w, h);
}

static void show_log(GLhandleARB s)
{
    GLint max_length, length;
    GLcharARB *log;

    glGetObjectParameterivARB(s, GL_OBJECT_INFO_LOG_LENGTH_ARB, &max_length);
    log = (GLcharARB *) malloc(max_length * sizeof(GLcharARB));
    glGetInfoLogARB(s, max_length, &length, log);
    printf("%s", log);
    bugle_free(log);
}

static void show_compile_log(GLhandleARB s)
{
    GLint compiled;

    glGetObjectParameterivARB(s, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
    if (compiled) printf("Compile successful\n");
    else show_log(s);
}

static void show_validate_log(GLhandleARB p)
{
    GLint valid;

    glGetObjectParameterivARB(p, GL_OBJECT_VALIDATE_STATUS_ARB, &valid);
    if (valid) printf("Valid\n"); else printf("Invalid\n");
    show_log(p);
}

static void init(void)
{
    GLhandleARB v, p;
    const GLcharARB *code =
        "uniform vec3 col;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "    gl_FrontColor = vec4(col, 1.0);\n"
        "}\n";
    GLsizei length;
    GLint loc;
    GLfloat col[3] = {0.0f, 1.0f, 0.0f};

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    v = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    length = strlen(code);
    glShaderSourceARB(v, 1, &code, &length);
    glCompileShaderARB(v);
    show_compile_log(v);

    p = glCreateProgramObjectARB();
    glAttachObjectARB(p, v);
    glDeleteObjectARB(v);
    glLinkProgramARB(p);
    glUseProgramObjectARB(p);

    loc = glGetUniformLocationARB(p, "col");
    glUniform3fvARB(loc, 1, col);

    glValidateProgramARB(p);
    show_validate_log(p);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitWindowSize(640, 480);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow("Shader demo");
    glewInit();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    init();
    glutMainLoop();
    return 0;
}
