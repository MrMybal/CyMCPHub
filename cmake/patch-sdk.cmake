set(patch "${CMAKE_CURRENT_LIST_DIR}/CyMCPSDK-streamable-http.patch")
execute_process(COMMAND "${GIT}" -C "${SDK_SOURCE}" apply --reverse --check --ignore-whitespace "${patch}"
    RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
if(NOT already_applied EQUAL 0)
    execute_process(COMMAND "${GIT}" -C "${SDK_SOURCE}" apply --ignore-whitespace "${patch}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()
