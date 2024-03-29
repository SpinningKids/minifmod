﻿# CMakeList.txt : CMake project for minifmod, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

set(HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

set(PUBLIC_HEADER_FILES
  ${HEADER_DIR}/${TARGET_NAME}/minifmod.h
)

set(PRIVATE_HEADER_FILES
  ${SRC_DIR}/Mixer.h
  ${SRC_DIR}/mixer_clipcopy.h
  ${SRC_DIR}/mixer_fpu_ramp.h
  ${SRC_DIR}/Music.h
  ${SRC_DIR}/music_formatxm.h
  ${SRC_DIR}/Sound.h
  ${SRC_DIR}/system_file.h
  ${SRC_DIR}/system_memory.h
  ${SRC_DIR}/xmeffects.h
)

set(SRC_FILES
  ${SRC_DIR}/Fmusic.c
  ${SRC_DIR}/Fsound.c
  ${SRC_DIR}/mixer_clipcopy.c
  ${SRC_DIR}/mixer_fpu_ramp.c
  ${SRC_DIR}/music_formatxm.c
  ${SRC_DIR}/system_file.c
)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${PUBLIC_HEADER_FILES} ${PRIVATE_HEADER_FILES} ${SRC_FILES})
target_include_directories(${TARGET_NAME} PUBLIC ${HEADER_DIR})
target_link_libraries(${TARGET_NAME} PUBLIC winmm.lib)
