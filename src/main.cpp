#define SDL_MAIN_USE_CALLBACKS 1 // Use the callbacks instead of main()

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cglm/cglm.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif // __EMSCRIPTEN__

#include "file_utils.h"
#include "shader_program.h"

typedef struct
{
    SDL_Window *window;
    SDL_GLContext glContext;
    GLuint shaderProgram;
    GLuint vao, vbo;
    GLuint textTextureID;
    TTF_Font *font;
    int textW;
    int textH;
    mat4 mvpMatrix;
    GLint uMvpMatrixLocation;
    ImGuiIO* io;
    float scale_factor;
} App;

static GLuint createTextureFromSurface(SDL_Surface *surface)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h,
        0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
}

static void updateProjectionAndMVP(App *app)
{
    int windowW, windowH;
    SDL_GetWindowSizeInPixels(app->window, &windowW, &windowH);

    // Update OpenGL viewport to match the new pixel dimensions
    glViewport(0, 0, windowW, windowH);

    float windowAspect = (float)windowW / (float)windowH;

    mat4 projMatrix;

    // Instead of a hardcoded fixed vertical size, check orientation
    // or base the frustum bounds on the text's actual dimensions to prevent clipping
    float orthoSize = 300.0f;

    float left, right, bottom, top;

    if (windowW >= windowH)
    {
        // Landscape mode: fix vertical height, scale width by aspect ratio
        left = -orthoSize * windowAspect;
        right = orthoSize * windowAspect;
        bottom = -orthoSize;
        top = orthoSize;
    }
    else
    {
        // Portrait mode: fix horizontal width, scale height by inverse aspect ratio
        left = -orthoSize;
        right = orthoSize;
        bottom = -orthoSize / windowAspect;
        top = orthoSize / windowAspect;
    }

    glm_ortho(left, right, bottom, top, -1.0f, 1.0f, projMatrix);

    mat4 viewMatrix;
    vec3 eye = { 0.0f, 0.0f, 1.0f };
    vec3 center = { 0.0f, 0.0f, 0.0f };
    vec3 up = { 0.0f, 1.0f, 0.0f };
    glm_lookat(eye, center, up, viewMatrix);

    mat4 modelMatrix;
    glm_mat4_identity(modelMatrix);

    vec3 scaleFactors = { (float)app->textW, (float)app->textH, 1.0f };
    glm_scale(modelMatrix, scaleFactors);

    mat4 projViewMatrix;
    glm_mat4_mul(projMatrix, viewMatrix, projViewMatrix);
    glm_mat4_mul(projViewMatrix, modelMatrix, app->mvpMatrix);
}

// This function runs once at startup
SDL_AppResult SDL_AppInit(void **appState, int argc, char *argv[])
{
    App *app = (App *)SDL_malloc(sizeof(App));
    *appState = app;

#ifndef __EMSCRIPTEN__
    if (!SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60"))
    {
        SDL_Log("Failed to set a frame rate: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
#endif

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!TTF_Init())
    {
        SDL_Log("Couldn't initialize SDL_ttf: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); // Enable MULTISAMPLE
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8); // Can be 2, 4, 8 or 16

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    // Mobile and Web: Request OpenGL ES 3.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    // Windows/Desktop: Request OpenGL 3.3 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // Explicitly ask for forward compatibility for better driver support
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

    Uint32 flags = SDL_WINDOW_OPENGL;
    int w = 480; // Default width for Windows
    int h = 480; // Default height for Windows

#if __ANDROID__
    flags |= (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE);
    w = 0;
    h = 0;
#endif // __ANDROID__

    app->window = SDL_CreateWindow("SDL3, OpenGL", w, h, flags);
    if (!app->window)
    {
        SDL_Log("Couldn't create the window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app->glContext = SDL_GL_CreateContext(app->window);
    if (!app->glContext)
    {
        SDL_Log("Couldn't create the glContext: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#ifdef WIN32
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        SDL_Log("Failed to initialize OpenGL function pointers");
        return SDL_APP_FAILURE;
    }
#endif // WIN32

    // Load shader sources
#if defined(__ANDROID__)
    // On Android, the assets folder is the root
    char *vertexSource = readFile("shaders/texture.vert");
    char *fragmentSource = readFile("shaders/texture.frag");
#else
    // On Windows/Desktop, you might still use the "assets/" prefix
    // if a local folder structure keeps them there
    char *vertexSource = readFile("assets/shaders/texture.vert");
    char *fragmentSource = readFile("assets/shaders/texture.frag");
#endif

    // Validate that both loaded successfully
    if (vertexSource != NULL && fragmentSource != NULL)
    {
        // Pass the pointers to your creation function
        app->shaderProgram = createShaderProgram(vertexSource, fragmentSource);
        if (!app->shaderProgram)
        {
            return SDL_APP_FAILURE;
        }

        // Once the GPU has compiled the shaders, you can free the memory
        SDL_free(vertexSource);
        SDL_free(fragmentSource);
    }
    else
    {
        // Handle error: one or both shaders failed to load
        if (vertexSource)
            SDL_free(vertexSource);
        if (fragmentSource)
            SDL_free(fragmentSource);

        SDL_Log("Error: Could not load shader files.");
        return SDL_APP_FAILURE;
    }

    glClearColor(0.2f, 0.2f, 0.2f, 1.f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(app->shaderProgram);

    app->uMvpMatrixLocation = glGetUniformLocation(app->shaderProgram, "uMvpMatrix");

    GLint textureLocation = glGetUniformLocation(app->shaderProgram, "ourTexture");
    glUniform1i(textureLocation, 0); // Explicitly set "ourTexture" to use GL_TEXTURE0

    // VAO/VBO Setup (Unit Quad 0.0 to 1.0)
    float vertices[] = {
        // x, y, u, v (flipped)
        -0.5f, 0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 1.0f,

        -0.5f, 0.5f, 0.0f, 0.0f,
        0.5f, 0.5f, 1.0f, 0.0f,
        0.5f, -0.5f, 1.0f, 1.0f
    };
    glGenVertexArrays(1, &app->vao);
    glGenBuffers(1, &app->vbo);
    glBindVertexArray(app->vao);
    glBindBuffer(GL_ARRAY_BUFFER, app->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Stride is 4 floats (x, y, u, v) = 16 bytes
    GLsizei stride = 4 * sizeof(float);

    // Position (Location 0, 2 components: x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    // Texture Coordinates (Location 1, 2 components: u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Load Font
#if defined(__ANDROID__)
    app->font = TTF_OpenFont("fonts/LiberationSans-Regular.ttf", 36.0f);
#else
    app->font = TTF_OpenFont("assets/fonts/LiberationSans-Regular.ttf", 36.0f);
#endif

    if (!app->font)
    {
        SDL_Log("Font error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Render text to surface, then to GL texture
    SDL_Color color = { 170, 255, 195, 255 };
    SDL_Surface *s = TTF_RenderText_Blended_Wrapped(app->font,
        "Hello, SDL3_ttf and OpenGL ES 3.0!\n\nПривет, SDL3_ttf и OpenGL ES 3.0!", 0, color, 0);
    if (s)
    {
        // Convert surface to a guaranteed RGBA format
        SDL_Surface *converted = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(s); // Free the original

        if (converted)
        {
            app->textW = converted->w;
            app->textH = converted->h;
            app->textTextureID = createTextureFromSurface(converted);
            SDL_DestroySurface(converted);
        }
    }

    updateProjectionAndMVP(app);

    // Setup Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    app->io = &ImGui::GetIO(); (void)app->io;
    ImGui::StyleColorsDark();

    // Query SDL3 for the actual platform display scale 
    // (Handles Windows desktop DPI, Android screen densities, and Wasm/Browser pixel ratios)
    app->scale_factor = SDL_GetWindowDisplayScale(app->window);
    if (app->scale_factor <= 0.0f)
    {
        app->scale_factor = 1.0f;
    }

#if defined(__ANDROID__)
    // Android screens have much higher DPIs; boost the multiplier if needed
    app->scale_factor *= 1.0f; 
#endif

    // app->io->Fonts->AddFontDefault();

    // Load custom TTF font for ImGui safely across all platforms (including Android assets)
    const char* font_path = 
#if defined(__ANDROID__)
        "fonts/LiberationSans-Regular.ttf";
#else
        "assets/fonts/LiberationSans-Regular.ttf";
#endif

    size_t font_size = 0;
    void* font_data = SDL_LoadFile(font_path, &font_size);
    if (font_data)
    {
        // AddFontFromMemoryTTF takes ownership of the buffer and frees it automatically when the atlas is cleared
        app->io->Fonts->AddFontFromMemoryTTF(font_data, (int)font_size, 24.0f * app->scale_factor);
    }
    else
    {
        SDL_Log("Failed to load ImGui font from %s: %s", font_path, SDL_GetError());
        app->io->Fonts->AddFontDefault();
    }

    ImGui::GetStyle().ScaleAllSizes(app->scale_factor);

    // Determine the GLSL version string based on the target platform
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    const char* glsl_version = "#version 300 es";
#else
    const char* glsl_version = "#version 330 core";
#endif

    // Initialize ImGui SDL3 backend
    ImGui_ImplSDL3_InitForOpenGL(app->window, app->glContext);

    // Initialize ImGui OpenGL3 backend with the dynamic version string
    ImGui_ImplOpenGL3_Init(glsl_version);

    return SDL_APP_CONTINUE;
}

// This function runs when a new event (mouse input, keypresses, etc) occurs
SDL_AppResult SDL_AppEvent(void *appState, SDL_Event *event)
{
    App *app = (App *)appState;

    // Forward events to ImGui SDL3 backend
    ImGui_ImplSDL3_ProcessEvent(event);

    switch (event->type)
    {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            updateProjectionAndMVP(app);
            break;
        }
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        default:
            break;
    }

    return SDL_APP_CONTINUE;
}

// This function runs once per frame, and is the heart of the program
SDL_AppResult SDL_AppIterate(void *appState)
{
    App *app = (App *)appState;

    // Start ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Set a default comfortable size for touch screens on first boot
    ImGui::SetNextWindowSize(ImVec2(200.0f * app->scale_factor, 100.0f * app->scale_factor), ImGuiCond_FirstUseEver);

    ImGui::Begin("Hello/Привет");
    ImGui::End();

    ImGui::Render();

    glViewport(0, 0, (int)app->io->DisplaySize.x, (int)app->io->DisplaySize.y);

    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->textTextureID);
    glBindVertexArray(app->vao);
    glUniformMatrix4fv(app->uMvpMatrixLocation, 1, GL_FALSE,
        (const GLfloat *)app->mvpMatrix);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // Update the screen
    SDL_GL_SwapWindow(app->window);
    return SDL_APP_CONTINUE;
}

// This function runs once at shutdown
void SDL_AppQuit(void *appState, SDL_AppResult result)
{
    App *app = (App *)appState;
    if (!app)
        return;

    // Clean up OpenGL resources
    glDeleteProgram(app->shaderProgram);
    glDeleteVertexArrays(1, &app->vao);
    glDeleteBuffers(1, &app->vbo);
    glDeleteTextures(1, &app->textTextureID);

    // Clean up SDL_ttf font resources (before calling TTF_Quit)
    if (app->font)
        TTF_CloseFont(app->font);

    // Destroy SDL Window and GL Context
    SDL_GL_DestroyContext(app->glContext);
    SDL_DestroyWindow(app->window);

    // Cleanup Contexts
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    // Quit subsystems in reverse order of initialization
    TTF_Quit();
    SDL_Quit();

    // Finally, free the app state memory
    SDL_free(app);
}
