cmake_minimum_required(VERSION 2.8)
add_executable(trafikk main.cpp controller.cpp controlleruser.cpp startup_sound.cpp)
target_link_libraries(trafikk sfml-graphics sfml-window sfml-system sfml-audio)

if(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()

