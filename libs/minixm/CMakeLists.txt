# CMakeList.txt : CMake project for minifmod, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

set(HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

set(PUBLIC_HEADER_FILES
  ${HEADER_DIR}/${TARGET_NAME}/channel.h
  ${HEADER_DIR}/${TARGET_NAME}/instrument.h
  ${HEADER_DIR}/${TARGET_NAME}/envelope.h
  ${HEADER_DIR}/${TARGET_NAME}/lfo.h
  ${HEADER_DIR}/${TARGET_NAME}/mixer.h
  ${HEADER_DIR}/${TARGET_NAME}/mixer_channel.h
  ${HEADER_DIR}/${TARGET_NAME}/module.h
  ${HEADER_DIR}/${TARGET_NAME}/pattern.h
  ${HEADER_DIR}/${TARGET_NAME}/playback.h
  ${HEADER_DIR}/${TARGET_NAME}/player_state.h
  ${HEADER_DIR}/${TARGET_NAME}/portamento.h
  ${HEADER_DIR}/${TARGET_NAME}/position.h
  ${HEADER_DIR}/${TARGET_NAME}/sample.h
  ${HEADER_DIR}/${TARGET_NAME}/system_file.h
  ${HEADER_DIR}/${TARGET_NAME}/xmeffects.h
)

set(PRIVATE_HEADER_FILES
)

set(SRC_FILES
  ${SRC_DIR}/channel.cpp
  ${SRC_DIR}/envelope.cpp
  ${SRC_DIR}/mixer.cpp
  ${SRC_DIR}/mixer_channel.cpp
  ${SRC_DIR}/module.cpp
  ${SRC_DIR}/playback.cpp
  ${SRC_DIR}/player_state.cpp
)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${PUBLIC_HEADER_FILES} ${PRIVATE_HEADER_FILES} ${SRC_FILES})
target_include_directories(${TARGET_NAME} PUBLIC ${HEADER_DIR})
target_link_libraries(${TARGET_NAME} PUBLIC xmformat)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
