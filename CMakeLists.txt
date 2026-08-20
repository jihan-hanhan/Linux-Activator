cmake_minimum_required(VERSION 3.16)
project(activate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(activate src/activate.cpp)
target_link_libraries(activate PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

add_executable(activate-daemon src/activate-daemon.cpp)
target_link_libraries(activate-daemon PRIVATE Qt6::Core)
