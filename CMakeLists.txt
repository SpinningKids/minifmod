# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("minifmod")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_compile_options(
		"$<$<CONFIG:RELEASE>:/Oxsi>"
		"$<$<CONFIG:RELEASE>:/Ob3>"
		"$<$<CONFIG:RELEASE>:/FAs>"
		"$<$<CONFIG:RELEASE>:/GL>"
		"$<$<CONFIG:RELEASE>:/GS->"
		"$<$<CONFIG:RELEASE>:/GR->"
		"$<$<CONFIG:RELEASE>:/Gr>"
		"$<$<CONFIG:RELEASE>:/arch:SSE2>"
		"$<$<CONFIG:RELEASE>:/fp:fast>"
		"$<$<CONFIG:RELEASE>:/Gw>"
		"$<$<CONFIG:RELEASE>:/EHs-c->"
		"$<$<CONFIG:RELEASE>:/sdl->"
		"$<$<CONFIG:RELEASE>:/vd0>"
		"/d2noftol3"
	)
	add_link_options(
		"/MAP:out.map"
		"$<$<CONFIG:RELEASE>:/LTCG>"
	)
endif()

# Include sub-projects.
add_subdirectory ("libs")
add_subdirectory ("apps")
