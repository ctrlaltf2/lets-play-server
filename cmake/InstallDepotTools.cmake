# Attempt to find depot_tools in either system path or in the in-source one
find_program(GCLIENT NAMES gclient HINTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/depot_tools)
find_package(Git QUIET)

set(MESSAGE_HEADER "[depot-tools-install]")

if(NOT GIT_FOUND)
    message(FATAL_ERROR "${MESSAGE_HEADER} Git not found. Please install it and try building again.")
endif()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools)
endif()

if(NOT GCLIENT)
    message(STATUS "${MESSAGE_HEADER} gclient not detected.")

    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/depot_tools)
        message(STATUS "${MESSAGE_HEADER} Fetching https://chromium.googlesource.com/chromium/tools/depot_tools.git...")
        execute_process(COMMAND ${GIT_EXECUTABLE} clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools
                        RESULT_VARIABLE GIT_DEPOT_TOOLS_RESULT
                        OUTPUT_FILE "/proc/self/fd/0")
        if(NOT GIT_DEPOT_TOOLS_RESULT EQUAL "0")
            message(FATAL_ERROR "${MESSAGE_HEADER} 'git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git' failed with error code ${GIT_DEPOT_TOOLS_RESULT}. Please attempt to fix the error or manually run the command inside ${CMAKE_CURRENT_SOURCE_DIR}/tools.")
        endif()
    endif()
endif()

set(DEPOT_TOOLS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tools/depot_tools)
