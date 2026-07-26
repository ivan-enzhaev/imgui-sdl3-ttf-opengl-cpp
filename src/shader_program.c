#include "shader_program.h"
#include <SDL3/SDL.h>

// Helper to wrap shader source with platform-specific definitions
char *get_final_shader_source(const char *source)
{
    const char *header =
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
        "#version 300 es\nprecision mediump float;\n";
#else
        "#version 330 core\n";
#endif

    size_t headerLen = SDL_strlen(header);
    size_t sourceLen = SDL_strlen(source);

    // Allocate memory for header + source + null terminator
    char *finalSource = (char *)SDL_malloc(headerLen + sourceLen + 1);
    if (!finalSource)
        return NULL;

    SDL_memcpy(finalSource, header, headerLen);
    SDL_memcpy(finalSource + headerLen, source, sourceLen);
    finalSource[headerLen + sourceLen] = '\0';

    return finalSource;
}

GLuint compile_shader(const char *source, GLenum shaderType)
{
    // Inject the header before compiling
    char *finalSource = get_final_shader_source(source);
    if (!finalSource)
        return 0;

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const GLchar **)&finalSource, NULL);
    glCompileShader(shader);

    // Free the injected source after compilation
    SDL_free(finalSource);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        SDL_Log("Shader compilation failed:\n%s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint createShaderProgram(const char *vertexSource, const char *fragmentSource)
{
    GLuint vertexShader = compile_shader(vertexSource, GL_VERTEX_SHADER);
    if (!vertexShader)
    {
        return 0;
    }

    GLuint fragmentShader = compile_shader(fragmentSource, GL_FRAGMENT_SHADER);
    if (!fragmentShader)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        SDL_Log("Shader program linking failed:\n%s\n", infoLog);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(program);

    return program;
}
