#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Reads a file into a null-terminated string.
 * The caller is responsible for calling SDL_free() on the returned pointer.
 */
char *readFile(const char *path)
{
    // Open the file for reading in binary mode
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io)
    {
        SDL_Log("Failed to open the file: %s (%s)", path, SDL_GetError());
        return NULL;
    }

    // Get the file size
    Sint64 size = SDL_GetIOSize(io);
    if (size < 0)
    {
        SDL_CloseIO(io);
        return NULL;
    }

    // Allocate memory for the content + null terminator
    char *content = (char *)SDL_malloc((size_t)size + 1);
    if (!content)
    {
        SDL_CloseIO(io);
        return NULL;
    }

    // Read the data
    size_t bytesRead = SDL_ReadIO(io, content, (size_t)size);
    content[bytesRead] = '\0'; // Null-terminate

    // Clean up
    SDL_CloseIO(io);

    if (bytesRead != (size_t)size)
    {
        SDL_free(content);
        return NULL;
    }

    return content;
}
