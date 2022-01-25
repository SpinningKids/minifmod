# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("minifmod")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		add_definitions("/O1i /FAs /GL /EHs-a- /GS- /GR-")
		SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} /MAP:out.map")
	endif()
endif()

# Include sub-projects.
add_subdirectory ("libs")
add_subdirectory ("apps")
