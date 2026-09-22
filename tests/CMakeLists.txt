set(S ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(F ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

function(add_player_test NAME)
    cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_ERR" "ARGS" ${ARGN})
    set(ARG_LINE "${ARG_ARGS}")
    string(REPLACE ";" "\\;" ARG_LINE_ESC "${ARG_LINE}")
    string(REPLACE ";" "\\;" EXPECT_OUT_ESC "${ARG_EXPECT_OUT}")
    string(REPLACE ";" "\\;" EXPECT_ERR_ESC "${ARG_EXPECT_ERR}")
    add_test(NAME ${NAME}
        COMMAND ${CMAKE_COMMAND}
            -DTEST_NAME=${NAME}
            -DPLAYER_COMMAND=$<TARGET_FILE:player>
            -DPLAYER_UNDER_NODE=$<BOOL:${EMSCRIPTEN}>
            -DARG_LINE=${ARG_LINE_ESC}
            -DEXPECT=${ARG_EXPECT}
            -DEXPECT_OUT=${EXPECT_OUT_ESC}
            -DEXPECT_ERR=${EXPECT_ERR_ESC}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
endfunction()

if(NOT EMSCRIPTEN)
add_player_test(smoke_log ARGS "--script;${S}/s_log.js" EXPECT 0)
add_player_test(smoke_quit3 ARGS "--script;${S}/s_quit3.js" EXPECT 3)
add_player_test(smoke_args ARGS "--script;${S}/s_args.js;one;two" EXPECT 0)
add_player_test(smoke_portable ARGS "--script;${S}/s_portable.js" EXPECT 0)
add_player_test(smoke_throw ARGS "--script;${S}/s_throw.js" EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
add_player_test(smoke_missing_file ARGS "--script;${S}/does_not_exist.js" EXPECT nonzero EXPECT_ERR "cannot read script file")
add_player_test(smoke_no_script_arg ARGS --script EXPECT nonzero EXPECT_ERR "requires a file argument")
add_player_test(smoke_root_ok ARGS "${F}/root_ok" EXPECT 0 EXPECT_OUT "entry-ok")
add_player_test(smoke_root_no_entry ARGS "${F}/root_no_entry" EXPECT nonzero EXPECT_ERR "no main.js in resource root")
add_player_test(smoke_root_missing_dir ARGS "${F}/no_such_dir" EXPECT nonzero EXPECT_ERR "not a directory")
add_player_test(smoke_root_eval_throw ARGS "${F}/web/root_eval_throw" EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
add_player_test(smoke_no_args EXPECT nonzero EXPECT_ERR "usage")
add_player_test(smoke_unknown_option ARGS --frobnicate EXPECT nonzero EXPECT_ERR "unknown option")
add_player_test(smoke_2d_validation ARGS "--script;${S}/s_2d_validation.js" EXPECT 0)
add_player_test(smoke_resource_lifecycle ARGS "--script;${S}/s_resource_lifecycle.js" EXPECT 0)
endif()

# web native-runtime tests (ADR 0022): the host JS engine (Node in this
# harness) runs the same portable scripts through the resource-root entry,
# asserting the same observable effects as the desktop --script suite
if(EMSCRIPTEN)
    function(add_web_test NAME STAGE_SCRIPT)
        cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_ERR;ROOT" "ARGS" ${ARGN})
        set(ARG_LINE "${ARG_ARGS}")
        string(REPLACE ";" "\\;" ARG_LINE_ESC "${ARG_LINE}")
        string(REPLACE ";" "\\;" EXPECT_OUT_ESC "${ARG_EXPECT_OUT}")
        string(REPLACE ";" "\\;" EXPECT_ERR_ESC "${ARG_EXPECT_ERR}")
        set(EXTRA_ARGS "")
        if(ARG_ROOT)
            list(APPEND EXTRA_ARGS "-DROOT_PATH=${ARG_ROOT}")
        elseif(IS_DIRECTORY "${STAGE_SCRIPT}")
            list(APPEND EXTRA_ARGS "-DROOT_PATH=${STAGE_SCRIPT}")
        else()
            list(APPEND EXTRA_ARGS "-DSTAGE_SCRIPT=${STAGE_SCRIPT}" "-DBINARY_DIR=${CMAKE_BINARY_DIR}")
        endif()
        add_test(NAME ${NAME}
            COMMAND ${CMAKE_COMMAND}
                -DTEST_NAME=${NAME}
                -DPLAYER_COMMAND=$<TARGET_FILE:player>
                -DPLAYER_UNDER_NODE=ON
                ${EXTRA_ARGS}
                -DARG_LINE=${ARG_LINE_ESC}
                -DEXPECT=${ARG_EXPECT}
                -DEXPECT_OUT=${EXPECT_OUT_ESC}
                -DEXPECT_ERR=${EXPECT_ERR_ESC}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    endfunction()

    set(WEBF ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/web)
    add_web_test(web_root_ok ${F}/root_ok EXPECT 0 EXPECT_OUT "entry-ok")
    add_web_test(web_root_no_entry ${F}/root_no_entry EXPECT nonzero EXPECT_ERR "no main.js in resource root")
    add_web_test(web_root_missing_dir missing EXPECT nonzero EXPECT_ERR "not a directory" ROOT ${F}/no_such_dir)
    add_web_test(web_root_frame ${WEBF}/root_frame EXPECT 0 EXPECT_OUT "frame-1")
    add_web_test(web_root_hooks_order ${WEBF}/root_hooks_order EXPECT 0 EXPECT_OUT "u1.*r1.*u2")
    add_web_test(web_root_hook_throw ${WEBF}/root_hook_throw EXPECT nonzero EXPECT_ERR "uncaught exception: hook boom")
    add_web_test(web_root_eval_throw ${WEBF}/root_eval_throw EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
    add_web_test(web_log ${S}/s_log.js EXPECT 0 EXPECT_OUT "smoke-log-ok")
    add_web_test(web_quit3 ${S}/s_quit3.js EXPECT 3 EXPECT_OUT "before-quit")
    add_web_test(web_args ${S}/s_args.js ARGS one;two EXPECT 0)
    add_web_test(web_portable ${S}/s_portable.js EXPECT 0)
    add_web_test(web_throw ${S}/s_throw.js EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
    add_web_test(web_2d_validation ${S}/s_2d_validation.js EXPECT 0)
    add_web_test(web_resource_lifecycle ${S}/s_resource_lifecycle.js EXPECT 0)
endif()

# headless unit tests (display list + JS API against a mock GPU sink)
function(add_unit_test TARGET CASE)
    if(EMSCRIPTEN)
        add_test(NAME ${TARGET}_${CASE}
            COMMAND node $<TARGET_FILE:${TARGET}> ${CASE})
    else()
        add_test(NAME ${TARGET}_${CASE} COMMAND ${TARGET} ${CASE})
    endif()
endfunction()

foreach(CASE
    compose_camera compose_quad value_snapshot default_camera_viewport
    blend_snapshot record_budget texture_lifecycle record_fields batching)
    add_unit_test(efx_render_tests ${CASE})
endforeach()
if(NOT EMSCRIPTEN)
foreach(CASE
    white quad_record camera_snapshot src_oob budget texture_lifecycle
    blend_snapshot default_camera)
    add_unit_test(efx_api_tests ${CASE})
endforeach()
endif()

# golden-image tests (require a GPU/display; enable in CI or locally with a
# running window system) — Emscripten goldens run through the browser job
option(EFX_BUILD_GOLDEN_TESTS "Build/run golden-image capture tests" OFF)
if(EFX_BUILD_GOLDEN_TESTS AND NOT EMSCRIPTEN)
    # embeds stb implementations -> relaxed warnings for this tool
    add_executable(efx_imgdiff imgdiff.c)
    target_include_directories(efx_imgdiff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb)
    if(UNIX)
        target_link_libraries(efx_imgdiff PRIVATE m)
    endif()
    if(MSVC)
        target_compile_options(efx_imgdiff PRIVATE /W3)
    else()
        target_compile_options(efx_imgdiff PRIVATE -Wall -Wno-unused-function)
    endif()

    # a scene's test activates once its golden image is committed (the
    # golden bootstrap produces them via the manual workflow job)
    foreach(SCENE clear solid srcrect pivot blend camera)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/goldens/${SCENE}/golden.png)
            add_test(NAME golden_${SCENE}
                COMMAND ${CMAKE_COMMAND}
                    -DGOLDEN_NAME=${SCENE}
                    -DGOLDEN_PLAYER=$<TARGET_FILE:player>
                    -DGOLDEN_IMGDIFF=$<TARGET_FILE:efx_imgdiff>
                    -DGOLDEN_SCENE=${CMAKE_CURRENT_SOURCE_DIR}/goldens/${SCENE}
                    -DBINARY_DIR=${CMAKE_BINARY_DIR}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_golden.cmake)
        else()
            message(STATUS "golden test deferred (no golden.png yet): ${SCENE}")
        endif()
    endforeach()
endif()

if(EFX_BUILD_DEV_HARNESS AND NOT EMSCRIPTEN)
    function(add_harness_test NAME SCRIPT OUT_RE ERR_RE)
        string(REPLACE ";" "\\;" ARG_LINE_ESC "--frames;12;${SCRIPT}")
        string(REPLACE ";" "\\;" OUT_RE_ESC "${OUT_RE}")
        string(REPLACE ";" "\\;" ERR_RE_ESC "${ERR_RE}")
        add_test(NAME ${NAME}
            COMMAND ${CMAKE_COMMAND}
                -DTEST_NAME=${NAME}
                -DPLAYER_COMMAND=$<TARGET_FILE:efx_dev_harness>
                -DARG_LINE=${ARG_LINE_ESC}
                -DEXPECT=0
                -DEXPECT_OUT=${OUT_RE_ESC}
                -DEXPECT_ERR=${ERR_RE_ESC}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    endfunction()

    add_harness_test(hooks_order_and_quit ${S}/h_both.js
        "u1.*r1.*u3-quit"
        "hooks update=1 render=1.*quit=1 code=5 error=0")
    add_harness_test(hooks_render_optional ${S}/h_only_update.js
        ""
        "hooks update=1 render=0.*quit=1 code=0 error=0")
    add_harness_test(hooks_throw_stops ${S}/h_throw.js
        ""
        "uncaught exception: hook boom.*error=1")
endif()
