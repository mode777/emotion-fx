cmake_minimum_required(VERSION 3.21)
project(emotionfx C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/quickjs-ng EXCLUDE_FROM_ALL)

add_library(efx_core STATIC
    src/api/api.c
    src/runtime/runtime.c
)
target_include_directories(efx_core PUBLIC src)
target_link_libraries(efx_core PUBLIC qjs)
if(MSVC)
    target_compile_options(efx_core PRIVATE /W4 /WX)
else()
    target_compile_options(efx_core PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT MSVC)
    target_compile_definitions(efx_core PRIVATE _POSIX_C_SOURCE=200809L)
endif()

add_library(efx_platform STATIC src/platform/platform.c)
target_include_directories(efx_platform PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sokol)
if(MSVC)
    target_compile_options(efx_platform PRIVATE /W4)
elseif(NOT EMSCRIPTEN)
    target_compile_options(efx_platform PRIVATE -Wall -Wextra)
endif()
if(UNIX AND NOT APPLE)
    target_compile_definitions(efx_platform PRIVATE _POSIX_C_SOURCE=200809L)
endif()

add_executable(player
    src/main.c
    src/player/player.c
)
target_link_libraries(player PRIVATE efx_core efx_platform)

if(EMSCRIPTEN)
    set_target_properties(player PROPERTIES SUFFIX ".js")
    target_link_options(player PRIVATE -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -sMAX_WEBGL_VERSION=2 -sNODERAWFS=1)
elseif(APPLE)
    enable_language(OBJC)
    set_source_files_properties(src/platform/platform.c PROPERTIES LANGUAGE OBJC)
    target_link_libraries(efx_platform PRIVATE
        "-framework Cocoa" "-framework QuartzCore"
        "-framework Metal" "-framework MetalKit")
elseif(WIN32)
    target_compile_definitions(efx_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(player PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(efx_platform PRIVATE d3d11 dxgi user32 gdi32 shell32)
elseif(UNIX)
    find_path(EFX_X11_INCLUDE_DIR X11/Xlib.h REQUIRED)
    find_library(EFX_X11_LIBRARY X11 REQUIRED)
    find_library(EFX_XI_LIBRARY Xi REQUIRED)
    find_library(EFX_XCURSOR_LIBRARY Xcursor REQUIRED)
    find_path(EFX_GL_INCLUDE_DIR GL/gl.h REQUIRED)
    find_library(EFX_GL_LIBRARY GL REQUIRED)
    target_include_directories(efx_platform PRIVATE
        ${EFX_X11_INCLUDE_DIR} ${EFX_GL_INCLUDE_DIR})
    target_link_libraries(efx_platform PRIVATE
        ${EFX_X11_LIBRARY} ${EFX_XI_LIBRARY} ${EFX_XCURSOR_LIBRARY}
        ${EFX_GL_LIBRARY} m)
endif()

option(EFX_BUILD_DEV_HARNESS "Build the temporary dev harness (F1 implementation aid)" OFF)

enable_testing()
add_subdirectory(tests)

if(EFX_BUILD_DEV_HARNESS AND NOT EMSCRIPTEN)
    add_executable(efx_dev_harness
        tests/dev_harness.c
        src/player/player.c
    )
    target_link_libraries(efx_dev_harness PRIVATE efx_core efx_platform)
    if(MSVC)
        target_compile_definitions(efx_dev_harness PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(efx_dev_harness PRIVATE /W4 /WX)
    else()
        target_compile_options(efx_dev_harness PRIVATE -Wall -Wextra -Werror)
    endif()
endif()
