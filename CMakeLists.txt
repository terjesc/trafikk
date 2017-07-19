cmake_minimum_required(VERSION 2.8)

#add imgui-sfml
set(IMGUI_SFML_SOURCES "imgui-sfml/imgui-SFML.cpp")
include_directories(imgui-sfml)

#add imgui
set(IMGUI_SOURCES "imgui/imgui.cpp" "imgui/imgui_draw.cpp")
include_directories(imgui)

#compile trafikk
add_executable(trafikk main.cpp controller.cpp controlleruser.cpp startup_sound.cpp ${IMGUI_SFML_SOURCES} ${IMGUI_SOURCES})
target_link_libraries(trafikk sfml-graphics sfml-window sfml-system sfml-audio GL GLEW)

if(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()
