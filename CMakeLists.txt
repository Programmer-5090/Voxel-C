cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenGL REQUIRED)

# Add FastNoise2 subdirectory
add_subdirectory(includes/FastNoise2)

# Voxel World source files (specific to our needs)
set(VOXEL_WORLD_SOURCES
    "window.cpp"
    "shader.cpp"
    "voxel world/voxel_chunk.cpp"
    "voxel world/voxel_world.cpp"
    "voxel world/chunk_mesh.cpp"
    "voxel world/voxel_renderer.cpp"
    "heightmap_generator.cpp"
    "includes/glad/src/glad.c"
)

# Create executable
add_executable(${PROJECT_NAME} ${VOXEL_WORLD_SOURCES})

# Include directories (be very explicit)
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}/includes/glfw3
    ${CMAKE_SOURCE_DIR}/includes/FastNoise2/include
)

# Link libraries - try to use system GLFW first
find_library(GLFW_LIB 
    NAMES glfw3_debug glfw3_release glfw3 glfw
    PATHS 
        ${CMAKE_SOURCE_DIR}/lib/glfw3
        ${CMAKE_SOURCE_DIR}/lib/glfw3/lib
        ${CMAKE_SOURCE_DIR}/includes/glfw3/lib
    NO_DEFAULT_PATH
)

if(GLFW_LIB)
    target_link_libraries(${PROJECT_NAME} 
        OpenGL::GL
        ${GLFW_LIB}
        FastNoise2
    )
else()
    # Fallback - try to find system GLFW
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        target_link_libraries(${PROJECT_NAME} 
            OpenGL::GL
            glfw
            FastNoise2
        )
    else()
        message(FATAL_ERROR "GLFW library not found")
    endif()
endif()

# Windows-specific linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        gdi32
        user32
        kernel32
        shell32
    )
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)
