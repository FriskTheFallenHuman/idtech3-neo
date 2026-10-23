/*
===========================================================================
Hexen II port of the RTCW single-player GPL source.
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Distributed under the GNU General Public License, version 3 or later.
See COPYING.txt and the source distribution's additional terms.
===========================================================================
*/

#include "tr_glprogs.h"
#include "tr_local.h"

namespace {
constexpr GLenum VERTEX_SHADER = 0x8B31;
constexpr GLenum FRAGMENT_SHADER = 0x8B30;
constexpr GLenum COMPILE_STATUS = 0x8B81;
constexpr GLenum LINK_STATUS = 0x8B82;
constexpr GLenum FRAMEBUFFER = 0x8D40;
constexpr GLenum COLOR_ATTACHMENT = 0x8CE0;
constexpr GLenum FRAMEBUFFER_COMPLETE = 0x8CD5;

GLuint(APIENTRY *createShader)(GLenum);
void(APIENTRY *shaderSource)(GLuint, GLsizei, const char **, const GLint *);
void(APIENTRY *compileShader)(GLuint);
void(APIENTRY *getShaderiv)(GLuint, GLenum, GLint *);
void(APIENTRY *getShaderInfoLog)(GLuint, GLsizei, GLsizei *, char *);
void(APIENTRY *deleteShader)(GLuint);
GLuint(APIENTRY *createProgram)();
void(APIENTRY *attachShader)(GLuint, GLuint);
void(APIENTRY *linkProgram)(GLuint);
void(APIENTRY *getProgramiv)(GLuint, GLenum, GLint *);
void(APIENTRY *getProgramInfoLog)(GLuint, GLsizei, GLsizei *, char *);
void(APIENTRY *deleteProgram)(GLuint);
void(APIENTRY *useProgram)(GLuint);
GLint(APIENTRY *getUniformLocation)(GLuint, const char *);
void(APIENTRY *uniform1i)(GLint, GLint);
void(APIENTRY *uniform1f)(GLint, GLfloat);
void(APIENTRY *uniform2f)(GLint, GLfloat, GLfloat);
void(APIENTRY *uniform3fv)(GLint, GLsizei, const GLfloat *);
void(APIENTRY *uniform4fv)(GLint, GLsizei, const GLfloat *);
void(APIENTRY *uniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
void(APIENTRY *genFramebuffers)(GLsizei, GLuint *);
void(APIENTRY *deleteFramebuffers)(GLsizei, const GLuint *);
void(APIENTRY *bindFramebuffer)(GLenum, GLuint);
void(APIENTRY *framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
GLenum(APIENTRY *checkFramebufferStatus)(GLenum);
void(APIENTRY *blitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
                                GLenum);

bool ValidProc(PROC address) {
    return address && address != (PROC)1 && address != (PROC)2 && address != (PROC)3 &&
           address != (PROC)-1;
}

template <typename T>
bool LoadProc(T &destination, const char *name, const char *extension = nullptr) {
    PROC address = qwglGetProcAddress(name);
    if (!ValidProc(address) && extension) {
        address = qwglGetProcAddress(extension);
    }
    destination = ValidProc(address) ? reinterpret_cast<T>(address) : nullptr;
    if (!destination) {
        ri.Printf(PRINT_WARNING, "GLSL: missing OpenGL entry point %s\n", name);
    }
    return destination != nullptr;
}

GLuint Compile(const char *path, GLenum type) {
    void *source = nullptr;
    int length = ri.FS_ReadFile(path, &source);
    if (length <= 0 || !source) {
        if (source) {
            ri.FS_FreeFile(source);
        }
        ri.Error(ERR_FATAL, "GLSL: cannot read %s", path);
        return 0;
    }
    GLuint shader = createShader(type);
    const char *text = static_cast<const char *>(source);
    shaderSource(shader, 1, &text, &length);
    compileShader(shader);
    ri.FS_FreeFile(source);
    GLint compiled = 0;
    getShaderiv(shader, COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[4096] = {};
        getShaderInfoLog(shader, sizeof(log), nullptr, log);
        deleteShader(shader);
        ri.Error(ERR_FATAL, "GLSL: %s\n%s", path, log);
        return 0;
    }
    return shader;
}
} // namespace

bool R_InitGLPrograms() {
    bool ready = qglActiveTextureARB && qglClientActiveTextureARB;
#define LOAD(function, name) ready = LoadProc(function, name) && ready
    LOAD(createShader, "glCreateShader");
    LOAD(shaderSource, "glShaderSource");
    LOAD(compileShader, "glCompileShader");
    LOAD(getShaderiv, "glGetShaderiv");
    LOAD(getShaderInfoLog, "glGetShaderInfoLog");
    LOAD(deleteShader, "glDeleteShader");
    LOAD(createProgram, "glCreateProgram");
    LOAD(attachShader, "glAttachShader");
    LOAD(linkProgram, "glLinkProgram");
    LOAD(getProgramiv, "glGetProgramiv");
    LOAD(getProgramInfoLog, "glGetProgramInfoLog");
    LOAD(deleteProgram, "glDeleteProgram");
    LOAD(useProgram, "glUseProgram");
    LOAD(getUniformLocation, "glGetUniformLocation");
    LOAD(uniform1i, "glUniform1i");
    LOAD(uniform1f, "glUniform1f");
    LOAD(uniform2f, "glUniform2f");
    LOAD(uniform3fv, "glUniform3fv");
    LOAD(uniform4fv, "glUniform4fv");
    LOAD(uniformMatrix4fv, "glUniformMatrix4fv");
#undef LOAD
    ready = LoadProc(genFramebuffers, "glGenFramebuffers", "glGenFramebuffersEXT") && ready;
    ready =
        LoadProc(deleteFramebuffers, "glDeleteFramebuffers", "glDeleteFramebuffersEXT") && ready;
    ready = LoadProc(bindFramebuffer, "glBindFramebuffer", "glBindFramebufferEXT") && ready;
    ready = LoadProc(framebufferTexture2D, "glFramebufferTexture2D", "glFramebufferTexture2DEXT") &&
            ready;
    ready = LoadProc(checkFramebufferStatus, "glCheckFramebufferStatus",
                     "glCheckFramebufferStatusEXT") &&
            ready;
    LoadProc(blitFramebuffer, "glBlitFramebuffer", "glBlitFramebufferEXT");
    return ready;
}

void R_ShutdownGLPrograms() {
    if (useProgram) {
        useProgram(0);
    }
}

bool idGLProgram::Load(const char *vertexPath, const char *fragmentPath) {
    Shutdown();
    GLuint vertex = Compile(vertexPath, VERTEX_SHADER);
    GLuint fragment = Compile(fragmentPath, FRAGMENT_SHADER);
    handle = createProgram();
    attachShader(handle, vertex);
    attachShader(handle, fragment);
    linkProgram(handle);
    deleteShader(vertex);
    deleteShader(fragment);
    GLint linked = 0;
    getProgramiv(handle, LINK_STATUS, &linked);
    if (!linked) {
        char log[4096] = {};
        getProgramInfoLog(handle, sizeof(log), nullptr, log);
        Shutdown();
        ri.Error(ERR_FATAL, "GLSL: linking %s\n%s", fragmentPath, log);
        return false;
    }
    ri.Printf(PRINT_ALL, "GLSL: loaded %s\n", fragmentPath);
    return true;
}

void idGLProgram::Shutdown() {
    if (handle) {
        deleteProgram(handle);
        handle = 0;
    }
}

void idGLProgram::Bind() const {
    useProgram(handle);
}
void idGLProgram::Unbind() {
    useProgram(0);
}
int idGLProgram::Uniform(const char *name) const {
    return getUniformLocation(handle, name);
}
void idGLProgram::SetInt(int location, int value) const {
    uniform1i(location, value);
}
void idGLProgram::SetFloat(int location, float value) const {
    uniform1f(location, value);
}
void idGLProgram::SetVec2(int location, float x, float y) const {
    uniform2f(location, x, y);
}
void idGLProgram::SetVec3(int location, const float *value) const {
    uniform3fv(location, 1, value);
}
void idGLProgram::SetVec4(int location, const float *value) const {
    uniform4fv(location, 1, value);
}
void idGLProgram::SetMatrix(int location, const float *value) const {
    uniformMatrix4fv(location, 1, GL_FALSE, value);
}

bool idGLFramebuffer::Create(unsigned int colorTexture) {
    Shutdown();
    genFramebuffers(1, &handle);
    Bind();
    framebufferTexture2D(FRAMEBUFFER, COLOR_ATTACHMENT, GL_TEXTURE_2D, colorTexture, 0);
    qglDrawBuffer(COLOR_ATTACHMENT);
    bool complete = checkFramebufferStatus(FRAMEBUFFER) == FRAMEBUFFER_COMPLETE;
    BindDefault();
    if (!complete) {
        Shutdown();
        ri.Error(ERR_FATAL, "Screen-space VFX framebuffer is incomplete");
    }
    return complete;
}

bool idGLFramebuffer::CreateDepth(unsigned int depthTexture) {
    Shutdown();
    genFramebuffers(1, &handle);
    Bind();
    framebufferTexture2D(FRAMEBUFFER, 0x8d00 /* DEPTH_ATTACHMENT */, GL_TEXTURE_2D, depthTexture,
                         0);
    qglDrawBuffer(GL_NONE);
    qglReadBuffer(GL_NONE);
    const bool complete = checkFramebufferStatus(FRAMEBUFFER) == FRAMEBUFFER_COMPLETE;
    BindDefault();
    if (!complete) {
        Shutdown();
    }
    return complete;
}
unsigned int idGLProgram::Current() {
    GLint program = 0;
    qglGetIntegerv(0x8b8d /* CURRENT_PROGRAM */, &program);
    return static_cast<unsigned int>(program);
}
void idGLProgram::Restore(unsigned int program) {
    useProgram(program);
}
bool idGLFramebuffer::CreateColorDepth(unsigned int colorTexture, unsigned int depthTexture) {
    Shutdown();
    genFramebuffers(1, &handle);
    Bind();
    framebufferTexture2D(FRAMEBUFFER, COLOR_ATTACHMENT, GL_TEXTURE_2D, colorTexture, 0);
    framebufferTexture2D(FRAMEBUFFER, 0x8d00 /* DEPTH_ATTACHMENT */, GL_TEXTURE_2D, depthTexture,
                         0);
    qglDrawBuffer(COLOR_ATTACHMENT);
    const bool complete = checkFramebufferStatus(FRAMEBUFFER) == FRAMEBUFFER_COMPLETE;
    BindDefault();
    if (!complete) {
        Shutdown();
    }
    return complete;
}

bool idGLFramebuffer::CopyDepth(const idGLFramebuffer &source, int x, int y, int size) const {
    if (!blitFramebuffer) {
        return false;
    }
    bindFramebuffer(0x8ca8 /* READ_FRAMEBUFFER */, source.handle);
    bindFramebuffer(0x8ca9 /* DRAW_FRAMEBUFFER */, handle);
    blitFramebuffer(x, y, x + size, y + size, x, y, x + size, y + size, GL_DEPTH_BUFFER_BIT,
                    GL_NEAREST);
    Bind();
    return true;
}

void idGLFramebuffer::Shutdown() {
    if (handle) {
        deleteFramebuffers(1, &handle);
        handle = 0;
    }
}
void idGLFramebuffer::Bind() const {
    bindFramebuffer(FRAMEBUFFER, handle);
}
void idGLFramebuffer::BindDefault() {
    bindFramebuffer(FRAMEBUFFER, 0);
}
