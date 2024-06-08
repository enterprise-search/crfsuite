cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(CQDB)
enable_language(C)

add_library(cqdb src/cqdb.c src/lookup3.c)
include_directories(${PROJECT_SOURCE_DIR}/include)
if(CMAKE_COMPILER_IS_GNUCC)
    # Note: gcc does not implicitly set _POSIX_C_SOURCE or _XOPEN_SOURCE
    # when using -std=c99.
    # http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_02_01_01
    # We specify that we are POSIX.1-2001 compliant and XSI-conforming. We only
    # need to specify _XOPEN_SOURCE as _POSIX_C_SOURCE will be set implicitly.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -D_XOPEN_SOURCE=600")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()
