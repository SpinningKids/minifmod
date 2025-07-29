# CMakeList.txt : CMake project for minifmod, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

set(HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

set(PUBLIC_HEADER_FILES
  ${HEADER_DIR}/${TARGET_NAME}/minifmod.h
)

set(PRIVATE_HEADER_FILES
)

set(SRC_FILES
  ${SRC_DIR}/minifmod.cpp
)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${PUBLIC_HEADER_FILES} ${PRIVATE_HEADER_FILES} ${SRC_FILES})
target_include_directories(${TARGET_NAME} PUBLIC ${HEADER_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE minixm winmm_playback)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
