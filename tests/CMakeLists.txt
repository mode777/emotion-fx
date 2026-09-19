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
add_player_test(smoke_no_args EXPECT nonzero EXPECT_ERR "usage")
add_player_test(smoke_unknown_option ARGS --frobnicate EXPECT nonzero EXPECT_ERR "unknown option")

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
