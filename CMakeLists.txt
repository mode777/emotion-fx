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
    src/render/render.c
)
target_include_directories(efx_core PUBLIC src)
target_link_libraries(efx_core PUBLIC qjs)
if(UNIX)
    target_link_libraries(efx_core PUBLIC m)
endif()
if(MSVC)
    target_compile_options(efx_core PRIVATE /W4 /WX)
else()
    target_compile_options(efx_core PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT MSVC)
    target_compile_definitions(efx_core PRIVATE _POSIX_C_SOURCE=200809L)
endif()

add_library(efx_platform STATIC
    src/platform/platform.c
    src/platform/pipeline.c
    src/platform/capture.c
)
target_include_directories(efx_platform PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sokol
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
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

    add_executable(player_web src/main.c src/player/player.c)
    target_link_libraries(player_web PRIVATE efx_core efx_platform)
    set_target_properties(player_web PROPERTIES SUFFIX ".html")
    target_link_options(player_web PRIVATE
        -sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1 -sMAX_WEBGL_VERSION=2
        -sENVIRONMENT=web
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/examples/browser@/examples/browser"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_pre.js")

    # browser capture build for the Emscripten golden-image job (ADR 0020):
    # goldens scenes are preloaded to /goldens/<scene>, captures are written
    # to MEMFS /captures/<scene>.png and read back by tools/run_web_goldens.mjs
    if(EFX_BUILD_GOLDEN_TESTS)
        add_executable(player_web_golden src/main.c src/player/player.c)
        target_link_libraries(player_web_golden PRIVATE efx_core efx_platform)
        set_target_properties(player_web_golden PROPERTIES SUFFIX ".js")
        target_link_options(player_web_golden PRIVATE
            -sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1 -sMAX_WEBGL_VERSION=2
            -sENVIRONMENT=web
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_golden_pre.js"
            "--post-js=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_golden_post.js"
            "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens@/goldens")
    endif()
elseif(APPLE)
    enable_language(OBJC)
    set_source_files_properties(src/platform/platform.c src/platform/pipeline.c src/platform/capture.c PROPERTIES LANGUAGE OBJC)
    target_link_libraries(efx_platform PRIVATE
        "-framework Cocoa" "-framework QuartzCore"
        "-framework Metal" "-framework MetalKit")
elseif(WIN32)
    target_compile_definitions(efx_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(player PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(efx_platform PRIVATE d3d11 dxgi uuid user32 gdi32 shell32)
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

# headless display-list unit tests (ADR 0019 record/assert gate)
add_executable(efx_render_tests
    src/render/render.c
    tests/unit/render_tests.c
)
target_include_directories(efx_render_tests PRIVATE src)
if(UNIX)
    target_link_libraries(efx_render_tests PRIVATE m)
endif()
if(EMSCRIPTEN)
    target_link_options(efx_render_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
endif()
if(MSVC)
    target_compile_options(efx_render_tests PRIVATE /W4 /WX)
else()
    target_compile_options(efx_render_tests PRIVATE -Wall -Wextra -Werror)
endif()

# headless JS-API tests (mock GPU sink + real quickjs runtime + bindings)
add_executable(efx_api_tests
    src/api/api.c
    src/runtime/runtime.c
    src/render/render.c
    tests/unit/api_tests.c
)
target_include_directories(efx_api_tests PRIVATE src)
if(UNIX)
    target_link_libraries(efx_api_tests PRIVATE m)
endif()
if(EMSCRIPTEN)
    target_link_options(efx_api_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
endif()
target_link_libraries(efx_api_tests PRIVATE qjs)
if(MSVC)
    target_compile_options(efx_api_tests PRIVATE /W4 /WX)
    target_compile_definitions(efx_api_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(efx_api_tests PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT MSVC)
    target_compile_definitions(efx_api_tests PRIVATE _POSIX_C_SOURCE=200809L)
endif()

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
