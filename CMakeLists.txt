cmake_minimum_required(VERSION 3.1)

set(ROOT_PROJ_NAME CLisp)

project(${ROOT_PROJ_NAME})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

option(DEBUG "Debug Version" on)
option(LOG_USE_COLOR "Log use color" on)
option(DEBUG_GC "Debug GC" on)

if(DEBUG)
    add_definitions(-DDEBUG)
endif(DEBUG)

if(LOG_USE_COLOR)
    add_definitions(-DLOG_USE_COLOR)
endif(LOG_USE_COLOR)

# add_definitions(-DDEBUG_TRACE)
# add_definitions(-DENABLE_LOG_FILE)

# GC DEBUG
if(DEBUG_GC)
    # add_definitions(-DDEBUG_GC)
    # add_definitions(-DDEBUG_GC_DETAIL)
    # add_definitions(-DDEBUG_PRESS_GC)
    # add_definitions(-DDEBUG_TRACE_GC)
endif(DEBUG_GC)

set(ROOT_SOURCE_DIR ${PROJECT_SOURCE_DIR})

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O2")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

# ---------- FUNCTIONS ----------
function(get_static_lib_path SUB_FOLDER LIB_NAME DEBUG_OUT RELEASE_OUT)
    if (WIN32 OR MSVC)
        set(${DEBUG_OUT} "${PROJECT_SOURCE_DIR}/libs/${SUB_FOLDER}/win/debug/${LIB_NAME}.lib" PARENT_SCOPE)
        set(${RELEASE_OUT} "${PROJECT_SOURCE_DIR}/libs/${SUB_FOLDER}/win/release/${LIB_NAME}.lib" PARENT_SCOPE)
        message(STATUS "+ Get static lib debug: ${DEBUG_OUT}")
        message(STATUS "+ Get static lib release: ${RELEASE_OUT}")
    elseif (APPLE)
        set(${DEBUG_OUT} "${PROJECT_SOURCE_DIR}/libs/${SUB_FOLDER}/macos/debug/${LIB_NAME}.a" PARENT_SCOPE)
        set(${RELEASE_OUT} "${PROJECT_SOURCE_DIR}/libs/${SUB_FOLDER}/macos/release/${LIB_NAME}.a" PARENT_SCOPE)
        message(STATUS "+ Get static lib debug: ${DEBUG_OUT}")
        message(STATUS "+ Get static lib release: ${RELEASE_OUT}")
    elseif (UNIX)
        set(${DEBUG_OUT} "${PROJECT_SOURCE_DIR}/libs/${SUB_FOLDER}/unix/debug/${LIB_NAME}.a" PARENT_SCOPE)
        set(${RELEASE_OUT} "${PROJECT_SOURCE_DIR}/libs/${SUB_FOLDER}/unix/release/${LIB_NAME}.a" PARENT_SCOPE)
        message(STATUS "+ Get static lib debug: ${DEBUG_OUT}")
        message(STATUS "+ Get static lib release: ${RELEASE_OUT}")
    endif()
endfunction()

add_subdirectory(libs/rlib/src)
add_subdirectory(src)
