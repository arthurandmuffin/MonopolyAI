cmake_minimum_required(VERSION 3.16)
project(MonopolyAI)
add_subdirectory(agents)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

file(GLOB ENGINE_SRC "src/engine/*.cpp")

add_executable(monopoly_engine ${ENGINE_SRC})
