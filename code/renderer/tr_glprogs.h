/*
===========================================================================
Hexen II port of the RTCW single-player GPL source.
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Distributed under the GNU General Public License, version 3 or later.
See COPYING.txt and the source distribution's additional terms.
===========================================================================
*/

#ifndef TR_GLPROGS_H
#define TR_GLPROGS_H

// All handles belong to the current GL context. Shutdown precedes GLimp_Shutdown.
class idGLProgram {
  public:
    idGLProgram() = default;
    idGLProgram(const idGLProgram &) = delete;
    idGLProgram &operator=(const idGLProgram &) = delete;
    bool Load(const char *vertexPath, const char *fragmentPath);
    void Shutdown();
    void Bind() const;
    static void Unbind();
    static unsigned int Current();
    static void Restore(unsigned int program);
    int Uniform(const char *name) const;
    void SetInt(int location, int value) const;
    void SetFloat(int location, float value) const;
    void SetVec2(int location, float x, float y) const;
    void SetVec3(int location, const float *value) const;
    void SetVec4(int location, const float *value) const;
    void SetMatrix(int location, const float *value) const;

  private:
    unsigned int handle = 0;
};

class idGLFramebuffer {
  public:
    idGLFramebuffer() = default;
    idGLFramebuffer(const idGLFramebuffer &) = delete;
    idGLFramebuffer &operator=(const idGLFramebuffer &) = delete;
    bool Create(unsigned int colorTexture);
    bool CreateColorDepth(unsigned int colorTexture, unsigned int depthTexture);
    bool CreateDepth(unsigned int depthTexture);
    bool CopyDepth(const idGLFramebuffer &source, int x, int y, int size) const;
    void Shutdown();
    void Bind() const;
    static void BindDefault();

  private:
    unsigned int handle = 0;
};

bool R_InitGLPrograms();
void R_ShutdownGLPrograms();

#endif
