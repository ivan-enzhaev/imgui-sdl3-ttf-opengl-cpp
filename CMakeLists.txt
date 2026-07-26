set(CMAKE_BUILD_TYPE "Release")

cmake_minimum_required(VERSION 3.21)
project(imgui-sdl3-ttf-opengl-cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (EMSCRIPTEN)
    set(SDL3_DIR "C:/libs/SDL3-devel-3.4.12-wasm/lib/cmake/SDL3")
    set(SDL3_ttf_DIR "C:/libs/SDL3_ttf-devel-3.2.2-wasm/lib/cmake/SDL3_ttf")
    set(FREETYPE_INCLUDE_DIRS "C:/libs/freetype-2.14.3-wasm/include")
    set(FREETYPE_LIBRARY "C:/libs/freetype-2.14.3-wasm/lib/libfreetype.a")
elseif (WIN32 AND NOT ANDROID)
    set(SDL3_DIR "C:/libs/SDL3-devel-3.4.12-mingw/lib/cmake/SDL3")
    set(SDL3_ttf_DIR "C:/libs/SDL3_ttf-devel-3.2.2-mingw/lib/cmake/SDL3_ttf")
endif()

if(ANDROID)
    # Native library target for Android (.so)
    add_library(app SHARED)
else()
    # Executable for Desktop / Web
    add_executable(app)
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

target_include_directories(app PRIVATE
    C:/libs/imgui-1.92.9-mingw/include
    C:/libs/cglm-0.9.6/include
)

if (EMSCRIPTEN)
    target_link_options(app PRIVATE 
        "-Oz"                   # Maximize code size optimization
        "-sMALLOC=emmalloc"     # Use minimal memory allocator
        "-sUSE_SDL=0"           # Disable built-in SDL
        "-sMAX_WEBGL_VERSION=2" # Force WebGL 2.0 support
        "-sMIN_WEBGL_VERSION=2" # Do not fallback to WebGL 1.0
        "-sFULL_ES3=1"          # Enable GLES3 -> WebGL 2 mapping
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    )
endif()

if (WIN32)
    target_include_directories(app PRIVATE C:/libs/glad-0.1.36/opengl-3.3/include)

    target_sources(app
        PRIVATE
        C:/libs/glad-0.1.36/opengl-3.3/src/glad.c
    )

    # Copy the assets folder to the dist folder (clearing it first)
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
        add_custom_command(TARGET app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:app>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/assets"
                    "$<TARGET_FILE_DIR:app>/assets"
            COMMENT "Copying assets directory"
        )
    endif()

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(app PRIVATE -mconsole)
    else()
        target_link_options(app PRIVATE -mwindows)
    endif()
endif()

if(ANDROID)
    target_link_libraries(app PRIVATE EGL GLESv3)

    # Define the target Android assets directory
    set(ANDROID_ASSETS_DIR "${CMAKE_SOURCE_DIR}/android-project/app/src/main/assets")

    # Copy the assets folder to the Android project structure (clearing it first)
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
        add_custom_command(TARGET app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${ANDROID_ASSETS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/assets"
                    "${ANDROID_ASSETS_DIR}"
            COMMENT "Copying assets to Android project structure"
        )
    endif()
endif()

target_sources(app
PRIVATE
    C:/libs/imgui-1.92.9/imgui.cpp
    C:/libs/imgui-1.92.9/imgui_draw.cpp
    C:/libs/imgui-1.92.9/imgui_tables.cpp
    C:/libs/imgui-1.92.9/imgui_widgets.cpp
    C:/libs/imgui-1.92.9/backends/imgui_impl_opengl3.cpp
    C:/libs/imgui-1.92.9/backends/imgui_impl_sdl3.cpp
    src/file_utils.h
    src/file_utils.c
    src/main.cpp
    src/shader_program.h
    src/shader_program.c
)

target_link_libraries(app PRIVATE SDL3_ttf::SDL3_ttf SDL3::SDL3)
