# Runs one golden-image test (design D8/D9): capture a frame of a golden
# scene, compare against the committed golden under the tolerance policy.
#
# usage:
#   cmake -DGOLDEN_NAME=<name> -DGOLDEN_PLAYER=<player> -DGOLDEN_IMGDIFF=<imgdiff>
#         -DGOLDEN_SCENE=<dir> -DBINARY_DIR=<build-dir>
#         -P run_golden.cmake

if(NOT GOLDEN_NAME OR NOT GOLDEN_PLAYER OR NOT GOLDEN_IMGDIFF OR NOT GOLDEN_SCENE OR NOT BINARY_DIR)
    message(FATAL_ERROR "run_golden.cmake: missing arguments")
endif()

set(OUT_DIR ${BINARY_DIR}/goldens)
file(MAKE_DIRECTORY ${OUT_DIR})
set(ACTUAL ${OUT_DIR}/${GOLDEN_NAME}-actual.png)
set(DIFF ${OUT_DIR}/${GOLDEN_NAME}-diff.png)
set(GOLDEN ${GOLDEN_SCENE}/golden.png)

if(NOT EXISTS ${GOLDEN})
    message(FATAL_ERROR "golden test '${GOLDEN_NAME}': committed golden missing: ${GOLDEN}")
endif()

execute_process(
    COMMAND ${GOLDEN_PLAYER} --capture-frame 2 --capture-output ${ACTUAL} ${GOLDEN_SCENE}
    RESULT_VARIABLE RC
    ERROR_VARIABLE ERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "golden test '${GOLDEN_NAME}': capture failed (exit ${RC})\n${ERR}")
endif()

execute_process(
    COMMAND ${GOLDEN_IMGDIFF} ${ACTUAL} ${GOLDEN} ${DIFF}
    RESULT_VARIABLE RC
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE ERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "golden test '${GOLDEN_NAME}': mismatch\n${OUT}\n${ERR}\nartifacts: ${ACTUAL} ${DIFF}")
endif()
