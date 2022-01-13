# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("minifmod")

# Include sub-projects.
add_subdirectory ("libs")
add_subdirectory ("apps")
