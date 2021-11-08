cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 17)

project("pivumeter")

add_executable(pivumeter
    pivumeter.cpp
    device/phat-beat.cpp
)

target_link_libraries(pivumeter pulse gpiod)
