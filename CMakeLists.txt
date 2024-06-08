cmake_minimum_required(VERSION 3.20)
project(
    crfsuite
  VERSION 1.0
  LANGUAGES C)

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

include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/lib/cqdb/include ${PROJECT_SOURCE_DIR}/lib/liblbfgs/include)

set(LBFGS_USE_SSE ON)
set(LBFGS_lib_TARGET_NAME liblbfgs)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/liblbfgs/)


# find_package(LBFGS REQUIRED ${PROJECT_SOURCE_DIR}/lib/liblbfgs)
# include_directories(${liblbfgs_INCLUDE_DIR})

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/cqdb/)
# find_package(cqdb REQUIRED)
include_directories(${cqdb_INCLUDE_DIR})

FILE (GLOB
      SOURCES
      ${PROJECT_SOURCE_DIR}/lib/crf/src/*.c)

add_library(crfsuite STATIC ${SOURCES})
set_target_properties(crfsuite PROPERTIES PUBLIC_HEADER "crfsuite/include/crfsuite.h;crfsuite/include/crfsuite.hpp;crfsuite/include/crfsuite_api.hpp")
target_link_libraries(crfsuite ${cqdb_LIBRARIES} ${liblbfgs_LIBRARIES} m)
install(TARGETS crfsuite ARCHIVE DESTINATION lib PUBLIC_HEADER DESTINATION include)
export(PACKAGE crfsuite)


add_executable(main ${PROJECT_SOURCE_DIR}/frontend/dump.c
${PROJECT_SOURCE_DIR}/frontend/iwa.c
${PROJECT_SOURCE_DIR}/frontend/learn.c
${PROJECT_SOURCE_DIR}/frontend/main.c
${PROJECT_SOURCE_DIR}/frontend/option.c
${PROJECT_SOURCE_DIR}/frontend/reader.c
${PROJECT_SOURCE_DIR}/frontend/tag.c
)

target_link_libraries(main crfsuite cqdb liblbfgs)