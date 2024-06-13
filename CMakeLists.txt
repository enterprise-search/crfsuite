cmake_minimum_required(VERSION 3.20)
project(
    crfsuite
  VERSION 1.0.1
  LANGUAGES C CXX)

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
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crf1d_context.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crf1d_encode.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crf1d_feature.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crf1d_model.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crf1d_tag.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crfsuite.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/crfsuite_train.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/holdout.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/logging.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/params.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/train_arow.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/train_averaged_perceptron.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/train_l2sgd.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/train_lbfgs.cpp
      ${PROJECT_SOURCE_DIR}/lib/crf/src/train_passive_aggressive.cpp
)

# set_source_files_properties(${PROJECT_SOURCE_DIR}/lib/crf/src/quark.cpp PROPERTIES LANGUAGE CXX)


add_library(crfsuite STATIC ${SOURCES})
set_property(TARGET crfsuite PROPERTY CXX_STANDARD 20)

set_target_properties(crfsuite PROPERTIES PUBLIC_HEADER "crfsuite/include/crfsuite.h;crfsuite/include/crfsuite.hpp;crfsuite/include/crfsuite_api.hpp")
target_link_libraries(crfsuite ${cqdb_LIBRARIES} ${liblbfgs_LIBRARIES} m)
install(TARGETS crfsuite ARCHIVE DESTINATION lib PUBLIC_HEADER DESTINATION include)
export(PACKAGE crfsuite)


add_executable(main ${PROJECT_SOURCE_DIR}/frontend/dump.cpp
${PROJECT_SOURCE_DIR}/frontend/iwa.cpp
${PROJECT_SOURCE_DIR}/frontend/learn.cpp
${PROJECT_SOURCE_DIR}/frontend/main.cpp
${PROJECT_SOURCE_DIR}/frontend/option.cpp
${PROJECT_SOURCE_DIR}/frontend/reader.cpp
${PROJECT_SOURCE_DIR}/frontend/tag.cpp
)

target_link_libraries(main crfsuite cqdb liblbfgs)

set_property(TARGET main PROPERTY CXX_STANDARD 20)
