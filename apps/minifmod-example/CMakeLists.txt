# CMakeList.txt : CMake project for minifmod, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)


# Add source to this project's executable.
add_executable(${TARGET_NAME} "minifmod-example.cpp")
target_link_libraries(${TARGET_NAME} PUBLIC minifmod)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 17)
