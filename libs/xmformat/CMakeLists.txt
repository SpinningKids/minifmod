# CMakeList.txt : CMake project for minifmod, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

set(HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

set(PUBLIC_HEADER_FILES
  ${HEADER_DIR}/${TARGET_NAME}/effect.h
  ${HEADER_DIR}/${TARGET_NAME}/envelope_flags.h
  ${HEADER_DIR}/${TARGET_NAME}/envelope_point.h
  ${HEADER_DIR}/${TARGET_NAME}/file.h
  ${HEADER_DIR}/${TARGET_NAME}/header.h
  ${HEADER_DIR}/${TARGET_NAME}/instrument_header.h
  ${HEADER_DIR}/${TARGET_NAME}/instrument_vibrato_type.h
  ${HEADER_DIR}/${TARGET_NAME}/loopmode.h
  ${HEADER_DIR}/${TARGET_NAME}/note.h
  ${HEADER_DIR}/${TARGET_NAME}/pattern_cell.h
  ${HEADER_DIR}/${TARGET_NAME}/pattern_header.h
  ${HEADER_DIR}/${TARGET_NAME}/sample_header.h
)

# Add source to this project's executable.
add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME} INTERFACE ${HEADER_DIR})
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
