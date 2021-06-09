cmake_minimum_required(VERSION 3.17)
project(swamp_capture_file_example C)

set(CMAKE_C_STANDARD 99)

set(deps ../deps/)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()


if (isDebug)
    message("Debug build detected")
    add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
    set(CONFIGURATION_DEBUG 1)
endif()



include_directories(${deps}tiny-libc/src/include)
include_directories(${deps}flood-c/src/include)
include_directories(${deps}raff-c/src/include)
include_directories(${deps}clog/src/include)
include_directories(include)

file(GLOB_RECURSE deps_src FOLLOW_SYMLINKS
        "${deps}*/src/lib/*.c"
        )

file(GLOB_RECURSE lib_src FOLLOW_SYMLINKS
        "lib/*.c"
        )

add_executable(swamp_capture_file_example
    ${deps_src}
    ${lib_src}
    examples/main.c
)
target_link_libraries(swamp_capture_file_example m)