# CMakeList.txt : CMake project for minifmod, include source and define
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
  ${SRC_DIR}/channel.h
  ${SRC_DIR}/envelope.h
  ${SRC_DIR}/instrument.h
  ${SRC_DIR}/lfo.h
  ${SRC_DIR}/mixer.h
  ${SRC_DIR}/module.h
  ${SRC_DIR}/pattern.h
  ${SRC_DIR}/player_state.h
  ${SRC_DIR}/portamento.h
  ${SRC_DIR}/position.h
  ${SRC_DIR}/sample.h
  ${SRC_DIR}/system_file.h
  ${SRC_DIR}/xmeffects.h
  ${SRC_DIR}/xmfile.h
)

set(SRC_FILES
  ${SRC_DIR}/channel.cpp
  ${SRC_DIR}/envelope.cpp
  ${SRC_DIR}/mixer.cpp
  ${SRC_DIR}/minifmod.cpp
  ${SRC_DIR}/module.cpp
  ${SRC_DIR}/player_state.cpp
  ${SRC_DIR}/system_file.cpp
)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${PUBLIC_HEADER_FILES} ${PRIVATE_HEADER_FILES} ${SRC_FILES})
target_include_directories(${TARGET_NAME} PUBLIC ${HEADER_DIR})
target_link_libraries(${TARGET_NAME} PUBLIC winmm.lib)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
