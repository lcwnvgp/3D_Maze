cmake_minimum_required(VERSION 3.10)

project(3DMazeGame)

set(CMAKE_CXX_STANDARD 17)

# Set source files
set(SOURCES
    main.cpp
    VulkanContext.cpp
    model.cpp
    Camera.cpp
    tiny_obj_loader.cc
)

# Set include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/glfw3.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan-1.dll
)

# Copy shaders
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)

foreach(SHADER ${SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    configure_file(${SHADER} ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME} COPYONLY)
endforeach()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
