find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

add_library(greedy_agent SHARED greedy_agent.c)
add_library(random_agent SHARED random_agent.c)
add_library(neat_bridge SHARED neat_bridge.c)  # <-- Move this up

target_include_directories(greedy_agent PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(random_agent PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(neat_bridge PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(neat_bridge PRIVATE ${Python3_LIBRARIES})

target_compile_definitions(greedy_agent PRIVATE AGENT_BUILD)
target_compile_definitions(random_agent PRIVATE AGENT_BUILD)
target_compile_definitions(neat_bridge PRIVATE AGENT_BUILD)

set_target_properties(greedy_agent PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/agents
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/agents
)
set_target_properties(random_agent PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/agents
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/agents
)
set_target_properties(neat_bridge PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/agents
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/agents
)