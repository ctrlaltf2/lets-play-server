if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc)
endif()

# Store path as it stands now so it can be reset to normal after this file runs.
set(OLD_PATH $ENV{PATH})

# Gclient wants to use python2, but on some systems, the python executable in PATH is python3! Yay for language developers breaking backwards compatibility!
# To get around this issue (gclient expecting the python executable in PATH to be python 2 instead of 3) we will have to make a temporary directory with a link to python2 called python and add it to PATH.

# First check if we actually need to do this
find_program(PYTHON NAMES python)

# And git
find_package(Git QUIET)
if(NOT GIT_FOUND)
    message(FATAL_ERROR "${MESSAGE_HEADER} Git not found. Please install it and try building again.")
endif()

set(MESSAGE_HEADER "[webrtc-install]")

if(NOT PYTHON)
    message(FATAL_ERROR "${MESSAGE_HEADER} Python wasn't found. Please install it using your package manager and try again.")
endif()

execute_process(COMMAND ${PYTHON} --version
                OUTPUT_VARIABLE PYTHON_VERSION)

# Split by space
string(FIND ${PYTHON_VERSION} " " SPLIT_POINT)
MATH(EXPR SPLIT_POINT "${SPLIT_POINT}+1")
# Extract version major
string(SUBSTRING ${PYTHON_VERSION} ${SPLIT_POINT} 1 PYTHON_VERSION_MAJOR)

# If the python in PATH isn't python2, do redirect hack
if(NOT PYTHON_VERSION_MAJOR EQUAL "2")
    message(STATUS "${MESSAGE_HEADER} Python in PATH (${PYTHON}) is not Python 2 which is required by gclient. Attempting to mititgate...")
# find_package(Python2 REQUIRED COMPONENTS Interpretter) # CMake's FindPython2.cmake doesn't work! YaY!
    find_program(PYTHON2 NAMES python2 python2.7 python2.6 python2.5)

    if(NOT PYTHON2)
        message(FATAL_ERROR "${MESSAGE_HEADER} Python2 wasn't found. Please install it using your package manager and try again.")
    endif()

    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/python)
        # Make the directory
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools)
        # Symlink python2 to tools/python/python
        file(CREATE_LINK ${PYTHON2} ${CMAKE_CURRENT_SOURCE_DIR}/tools/python SYMBOLIC)
    endif()

    # Add tools/python to path and hope for the best
    set(ENV{PATH} "${CMAKE_CURRENT_SOURCE_DIR}/tools:$ENV{PATH}")

# fetch relies on gclient being in PATH, so we have to add depot_tools folder to it (we'll change it back at the end)
    set(ENV{PATH} "$ENV{PATH}:${DEPOT_TOOLS_PATH}")
endif()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc/src)
    message(STATUS "${MESSAGE_HEADER} Downloading WebRTC library. This will take a long time...")
    execute_process(COMMAND ${DEPOT_TOOLS_PATH}/fetch --nohooks webrtc
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc
                    RESULT_VARIABLE FETCH_WEBRTC_RESULT
                    OUTPUT_FILE "/proc/self/fd/0")

    if(NOT FETCH_WEBRTC_RESULT EQUAL "0")
        message(WARNING "${MESSAGE_HEADER} Fetching webrtc failed with error code ${FETCH_WEBRTC_RESULT}.")
    endif()
endif()

message(STATUS "${MESSAGE_HEADER} Checking out WebRTC version M76...")
execute_process(COMMAND ${GIT_EXECUTABLE} checkout -b branch-heads/m76
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc/src
                OUTPUT_FILE "proc/self/fd/0")

message(STATUS "${MESSAGE_HEADER} WebRTC version M76 checked out. Syncing...")
execute_process(COMMAND ${DEPOT_TOOLS_PATH}/gclient sync
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc
                OUTPUT_FILE "/proc/self/fd/0")

## Now it can be built!

# Gen Ninja build files
find_program(GN NAMES gn)
find_program(NINJA NAMES ninja)

if(NOT GN OR NOT NINJA)
    message(FATAL_ERROR "${MESSAGE_HEADER} WebRTC requires gn and ninja to build. Please install them and try again.")
endif()

message(STATUS "${MESSAGE_HEADER} Generating Ninja build files...")
execute_process(COMMAND ${GN} gen out/Default
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc/src
                OUTPUT_FILE "/proc/self/fd/0")

message(STATUS "${MESSAGE_HEADER} Beginning WebRTC compilation...")
execute_process(COMMAND ${NINJA} -C out/Default
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/webrtc/src
                OUTPUT_FILE "/proc/self/fd/0"
                ERROR_FILE "/proc/self/fd/0")


# Reset the PATH variable that we modified
set(ENV{PATH} ${OLD_PATH})
