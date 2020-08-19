set(PROJ_NAME cmal)

project(${PROJ_NAME})

aux_source_directory(. SRC_FILES)

file(GLOB HEADERS "*.h")
source_group("Headers" FILES ${HEADERS})

# set output path
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(EXECUTABLE_OUTPUT_PATH "${ROOT_SOURCE_DIR}/bin/${PROJ_NAME}/Debug")
else(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(EXECUTABLE_OUTPUT_PATH "${ROOT_SOURCE_DIR}/bin/${PROJ_NAME}/Release")
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

include_directories(${ROOT_SOURCE_DIR}/libs/rlib/src)

add_executable(
    ${PROJ_NAME}
    ${SRC_FILES}
    ${HEADERS}
)

target_link_libraries(
    ${PROJ_NAME}
    RLib
)
