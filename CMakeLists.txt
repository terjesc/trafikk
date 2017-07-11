cmake_minimum_required(VERSION 2.8)
add_executable(trafikk main.cpp controller.cpp controlleruser.cpp startup_sound.cpp ${IMGUI_SFML_SOURCES})
target_link_libraries(trafikk sfml-graphics sfml-window sfml-system sfml-audio imgui ${IMGUI_SFML_DEPENDENCIES})

if(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()

#add imgui-sfml
set(IMGUI_SFML_BUILD_EXAMPLES False)
set(IMGUI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_subdirectory(imgui-sfml)
include_directories(imgui-sfml)

#add imgui
add_subdirectory(imgui)
include_directories(imgui)
