cmake_minimum_required (VERSION 3.10)

add_subdirectory ("minifmod")
add_subdirectory ("minixm")
add_subdirectory ("xmformat")

if(WIN32)
add_subdirectory ("winmm_playback")
else()
add_subdirectory ("pulseaudio_playback")
endif()
