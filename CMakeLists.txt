cmake_minimum_required(VERSION 3.10)

project(3DMazeGame)

set(CMAKE_CXX_STANDARD 17)

# Find Vulkan package
find_package(Vulkan REQUIRED)

# Add Vulkan beta extensions
add_definitions(-DVK_ENABLE_BETA_EXTENSIONS)

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
    ${Vulkan_INCLUDE_DIRS}
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/glfw3.dll
    ${Vulkan_LIBRARIES}
    vulkan-1
)

# Shader SPIR-V compilation using file-level custom commands
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_BIN_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

set(SHADER_FILES
    raygen.rgen
    miss.rmiss
    closesthit.rchit
)

set(SPIRV_TARGETS)
foreach(SHADER_NAME IN LISTS SHADER_FILES)
    add_custom_command(
        OUTPUT ${SHADER_BIN_DIR}/${SHADER_NAME}.spv
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V -S ${SHADER_NAME} -o ${SHADER_BIN_DIR}/${SHADER_NAME}.spv ${SHADER_SRC_DIR}/${SHADER_NAME}
        DEPENDS ${SHADER_SRC_DIR}/${SHADER_NAME}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V"
    )
    list(APPEND SPIRV_TARGETS ${SHADER_BIN_DIR}/${SHADER_NAME}.spv)
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_TARGETS})
add_dependencies(${PROJECT_NAME} Shaders)

foreach(SHADER ${SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    configure_file(${SHADER} ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME} COPYONLY)
endforeach()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy DLLs to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/glfw3.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/vulkan-1.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
