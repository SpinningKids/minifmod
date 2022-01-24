# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("minifmod")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions("/O1i /FAs /GL /EHs-a- /GS- /GR-")
endif()

# Include sub-projects.
add_subdirectory ("libs")
add_subdirectory ("apps")
