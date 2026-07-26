#ifndef SHADER_PROGRAM_H
#define SHADER_PROGRAM_H

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif // __EMSCRIPTEN__

#ifdef __cplusplus
extern "C"
{
#endif

    GLuint createShaderProgram(const char *vertexSource, const char *fragmentSource);

#ifdef __cplusplus
}
#endif

#endif // SHADER_PROGRAM_H
