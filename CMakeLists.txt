cmake_minimum_required(VERSION 2.8)
add_executable(trafikk main.cpp controller.cpp controlleruser.cpp)
target_link_libraries(trafikk sfml-graphics sfml-window sfml-system)
#target_link_libraries()

if(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()

