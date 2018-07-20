cmake_minimum_required(VERSION 2.6)
project(ashigaru)

add_executable(ashigaru main.cpp)

install(TARGETS ashigaru RUNTIME DESTINATION bin)
