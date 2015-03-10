#include "glplugin.h"
#include <QPainter>
#include <stdio.h>
#include <QGraphicsScene>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QImage>

#define STRINGIFY(...) #__VA_ARGS__

static GLuint loadShader(GLenum type, const GLchar *shaderSrc)
{
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if (!shader)
        return 0;

    glShaderSource(shader, 1, &shaderSrc, 0);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

class ShaderProgram {
public:
    static ShaderProgram* instance() {
        if (!m_instance) {
            m_instance = new ShaderProgram();
        }
        return m_instance;
    }

    GLint program() { return m_program; }
    GLuint vertexAttr() { return m_vertexAttr; }

private:
    void initialize() {
        GLchar vShaderStr[] =
            STRINGIFY(
                attribute vec3 vertex;
                void main(void)
                {
                    gl_Position = vec4(vertex, 1.0);
                }
            );

        GLchar fShaderStr[] =
            STRINGIFY(
                void main(void)
                {
                    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
                }
            );

        GLuint vertexShader;
        GLuint fragmentShader;
        GLint linked;

        vertexShader = loadShader(GL_VERTEX_SHADER, vShaderStr);
        fragmentShader = loadShader(GL_FRAGMENT_SHADER, fShaderStr);
        if (!vertexShader || !fragmentShader)
            return;

        m_program = glCreateProgram();
        if (!m_program)
            return;

        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);

        glLinkProgram(m_program);
        glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
        if (!linked) {
            glDeleteProgram(m_program);
            m_program = 0;
        }

        m_vertexAttr = glGetAttribLocation(m_program, "vertex");
    }

    ShaderProgram()
        : m_program(0)
        , m_vertexAttr(0)
    {
        initialize();
    }

    static ShaderProgram *m_instance;
    GLint m_program;
    GLuint m_vertexAttr;
};
ShaderProgram* ShaderProgram::m_instance = NULL;


GLPlugin::GLPlugin()
    : QGraphicsWidget(0)
{
}

void GLPlugin::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QOpenGLContext *prev = QOpenGLContext::currentContext();

    QOffscreenSurface s;
    s.create();
    QOpenGLContext c;
    c.create();
    c.makeCurrent(&s);
    GLuint fbo, tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geometry().toAlignedRect().width(), geometry().toAlignedRect().height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    glViewport(0, 0, geometry().toAlignedRect().width(), geometry().toAlignedRect().height());

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    ShaderProgram *program = ShaderProgram::instance();
    glUseProgram(program->program());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    GLfloat afVertices[] = {
        -1, -1, 0,
        1, -1, 0,
        0,  1, 0
    };
    glVertexAttribPointer(program->vertexAttr(), 3, GL_FLOAT, GL_FALSE, 0, afVertices);
    glEnableVertexAttribArray(program->vertexAttr());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(program->vertexAttr());

    glFlush();

    QImage img(geometry().toAlignedRect().width(), geometry().toAlignedRect().height(), QImage::Format_RGBA8888_Premultiplied);
    glReadPixels(0,0,geometry().toAlignedRect().width(), geometry().toAlignedRect().height(),GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    if (prev)
        prev->makeCurrent(prev->surface());

    painter->drawImage(geometry(), img.mirrored());    
}