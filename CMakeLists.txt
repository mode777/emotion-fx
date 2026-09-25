cmake_minimum_required(VERSION 3.21)
project(emotionfx C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

if(EMSCRIPTEN)
    # EM_ASM requires GNU statement expressions
    set(CMAKE_C_EXTENSIONS ON)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT EMSCRIPTEN)
    # ADR 0022: quickjs never ships to the browser — the web player is the
    # C core + the page's native JS engine through the src/web/ bridge.
    set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/quickjs-ng EXCLUDE_FROM_ALL)
endif()

if(EMSCRIPTEN)
    add_library(efx_core STATIC
        src/render/render.c
        src/web/bridge.c
    )
else()
    add_library(efx_core STATIC
        src/api/api.c
        src/runtime/runtime.c
        src/render/render.c
    )
endif()
target_include_directories(efx_core PUBLIC src)
if(NOT EMSCRIPTEN)
    target_link_libraries(efx_core PUBLIC qjs)
endif()
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

add_library(efx_math STATIC
    src/math/efx_math.cpp
)
target_include_directories(efx_math PUBLIC src)
target_include_directories(efx_math SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
if(MSVC)
    # GLM headers are not /W4-clean; the wrapper TU itself is
    target_compile_options(efx_math PRIVATE /W3)
else()
    target_compile_options(efx_math PRIVATE -Wall -Wextra -Werror)
endif()

# headless developer builds: unit-test targets only — no sokol platform,
# player, or golden tests (the display-required targets need X11/GL on
# UNIX); the full build matrix is the verification server's and CI's job
option(EFX_HEADLESS "Build headless unit-test targets only" OFF)

if(NOT EFX_HEADLESS)
add_library(efx_platform STATIC
    src/platform/platform.c
    src/platform/pipeline.c
    src/platform/capture.c
)
target_include_directories(efx_platform PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sokol
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(efx_platform PUBLIC efx_math)
if(MSVC)
    target_compile_options(efx_platform PRIVATE /W4)
elseif(NOT EMSCRIPTEN)
    target_compile_options(efx_platform PRIVATE -Wall -Wextra)
endif()
if(UNIX AND NOT APPLE)
    target_compile_definitions(efx_platform PRIVATE _POSIX_C_SOURCE=200809L)
endif()

if(EMSCRIPTEN)
    add_executable(player src/web/web_main.c)
else()
    add_executable(player
        src/main.c
        src/player/player.c
    )
endif()
target_link_libraries(player PRIVATE efx_core efx_platform)

if(EMSCRIPTEN)
    set(EFX_WEB_COMMON
        -sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=2MB
        -sEXPORTED_FUNCTIONS=_main,_malloc,_free
        # the web bridge's JS (entry.js/boot_pre.js) uses the FS API at
        # runtime for resource roots; keep it linked even when no C code
        # touches files (nothing else forces FS inclusion on the node player)
        -sFORCE_FILESYSTEM=1
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js"
        "--post-js=${CMAKE_CURRENT_SOURCE_DIR}/src/web/entry.js")

    # Node-runnable web player (host-engine smoke/comparison harness; the
    # desktop-style --script mode is a desktop-only run mode)
    set_target_properties(player PROPERTIES
        SUFFIX ".js"
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/web/entry.js;${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js")
    target_link_options(player PRIVATE
        ${EFX_WEB_COMMON}
        -sMAX_WEBGL_VERSION=2
        -sNODERAWFS=1 -sENVIRONMENT=node)

    add_executable(player_web src/web/web_main.c)
    target_link_libraries(player_web PRIVATE efx_core efx_platform)
    set_target_properties(player_web PROPERTIES
        SUFFIX ".html"
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/web/entry.js;${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js;${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_pre.js;${CMAKE_CURRENT_SOURCE_DIR}/examples/browser/main.js")
    target_link_options(player_web PRIVATE
        ${EFX_WEB_COMMON}
        -sMAX_WEBGL_VERSION=2 -sENVIRONMENT=web
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/examples/browser@/examples/browser"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_pre.js")

    # browser capture build for the Emscripten golden-image job (ADR 0020):
    # goldens scenes are preloaded to /goldens/<scene>, harness fixture roots
    # to /fixtures/<name>; captures are exported via canvas.toDataURL and
    # read back by tools/run_web_goldens.mjs / tools/run_web_harness.mjs
    if(EFX_BUILD_GOLDEN_TESTS)
        add_executable(player_web_golden src/web/web_main.c)
        target_link_libraries(player_web_golden PRIVATE efx_core efx_platform)
        target_compile_definitions(player_web_golden PRIVATE EFX_WEB_GOLDEN=1)
        set_target_properties(player_web_golden PROPERTIES
            SUFFIX ".js"
            LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/web/entry.js;${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js")
        target_link_options(player_web_golden PRIVATE
            ${EFX_WEB_COMMON}
            -sMAX_WEBGL_VERSION=2 -sENVIRONMENT=web -sSTACK_SIZE=4MB
            "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens@/goldens"
            "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures@/fixtures")
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
endif() # NOT EFX_HEADLESS

option(EFX_BUILD_DEV_HARNESS "Build the temporary dev harness (F1 implementation aid)" OFF)

# regenerate shaders/quad.h + shaders/mesh.h when the pinned sokol-shdc
# binary is provided:
#   cmake -B build -DEFX_SHDC=/path/to/sokol-shdc ...
set(EFX_SHDC "" CACHE FILEPATH "Path to the pinned sokol-shdc binary (regenerates shaders/quad.h and shaders/mesh.h)")
if(EFX_SHDC)
    add_custom_target(regen_shaders
        COMMAND ${EFX_SHDC} -i ${CMAKE_CURRENT_SOURCE_DIR}/shaders/quad.glsl
                -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/quad.h
                --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl
        COMMAND ${EFX_SHDC} -i ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.glsl
                -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.h
                --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/quad.glsl
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.glsl
        COMMENT "Regenerating shaders/quad.h and shaders/mesh.h via sokol-shdc")
endif()

enable_testing()
add_subdirectory(tests)

# headless math wrapper tests (GLM behind the plain C API, ADR 0005)
add_executable(efx_math_tests
    src/math/efx_math.cpp
    tests/unit/math_tests.c
)
target_include_directories(efx_math_tests PRIVATE src)
target_include_directories(efx_math_tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
if(UNIX)
    target_link_libraries(efx_math_tests PRIVATE m)
endif()
if(MSVC)
    target_compile_options(efx_math_tests PRIVATE /W3)
else()
    target_compile_options(efx_math_tests PRIVATE -Wall -Wextra -Werror)
endif()

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

# headless JS-API tests (mock GPU sink + real quickjs runtime + bindings);
# desktop-only: ADR 0022 keeps quickjs out of every Emscripten target
if(NOT EMSCRIPTEN)
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
