if(NOT PLAYER_COMMAND OR NOT TEST_NAME)
    message(FATAL_ERROR "usage: cmake -DTEST_NAME=<name> -DPLAYER_COMMAND=<cmd> -DARG_LINE=<args...> -DEXPECT=<code|nonzero> [-DEXPECT_OUT=<regex>] [-DEXPECT_ERR=<regex>] [-DPLAYER_UNDER_NODE=on] -P run_test.cmake")
endif()

separate_arguments(ARG_LINE)
if(PLAYER_UNDER_NODE)
    find_program(SMOKE_NODE node REQUIRED)
    set(FULL_COMMAND ${SMOKE_NODE} ${PLAYER_COMMAND} ${ARG_LINE})
else()
    set(FULL_COMMAND ${PLAYER_COMMAND} ${ARG_LINE})
endif()

execute_process(
    COMMAND ${FULL_COMMAND}
    RESULT_VARIABLE RC
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE ERR
)

if(EXPECT STREQUAL "nonzero")
    if(RC EQUAL 0)
        message(FATAL_ERROR "test '${TEST_NAME}': expected non-zero exit, got 0\n--- stdout ---\n${OUT}\n--- stderr ---\n${ERR}")
    endif()
elseif(NOT RC EQUAL EXPECT)
    message(FATAL_ERROR "test '${TEST_NAME}': expected exit ${EXPECT}, got ${RC}\n--- stdout ---\n${OUT}\n--- stderr ---\n${ERR}")
endif()

if(EXPECT_OUT)
    if(NOT OUT MATCHES "${EXPECT_OUT}")
        message(FATAL_ERROR "test '${TEST_NAME}': stdout did not match '${EXPECT_OUT}'\n--- stdout ---\n${OUT}\n--- stderr ---\n${ERR}")
    endif()
endif()

if(EXPECT_ERR)
    if(NOT ERR MATCHES "${EXPECT_ERR}")
        message(FATAL_ERROR "test '${TEST_NAME}': stderr did not match '${EXPECT_ERR}'\n--- stdout ---\n${OUT}\n--- stderr ---\n${ERR}")
    endif()
endif()
