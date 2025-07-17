cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the path where we installed GLFW4 and Assimp
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/lib/glfw3" "${CMAKE_SOURCE_DIR}/lib/assimp" ${CMAKE_PREFIX_PATH})

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Try to find Assimp
find_package(assimp QUIET)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/includes)
include_directories(${CMAKE_SOURCE_DIR}/includes/glad)
include_directories(${CMAKE_SOURCE_DIR}/includes/glm)
include_directories(${CMAKE_SOURCE_DIR}/includes/assimp/include)

# Platform-specific include directories
if(WIN32)
    # Windows-specific includes - adjust these paths based on your setup
    # include_directories("C:/vcpkg/installed/x64-windows/include")
elseif(APPLE)
    include_directories(/opt/homebrew/include)
endif()

# Add all source files
file(GLOB SOURCES "*.cpp")
file(GLOB WORLD_SOURCES "world/*.cpp")
set(GLAD_SOURCES "${CMAKE_SOURCE_DIR}/includes/glad/src/glad.c")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${WORLD_SOURCES} ${GLAD_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    OpenGL::GL
    glfw
)

# Link Assimp
if(assimp_FOUND)
    target_link_libraries(${PROJECT_NAME} assimp::assimp)
else()
    # Fallback to manual linking if find_package fails
    if(WIN32)
        # Windows-specific library paths
        target_link_directories(${PROJECT_NAME} PRIVATE 
            ${CMAKE_SOURCE_DIR}/includes/assimp/lib
        )
    elseif(APPLE)
        # macOS-specific library paths
        target_link_directories(${PROJECT_NAME} PRIVATE 
            /opt/homebrew/lib
            ${CMAKE_SOURCE_DIR}/includes/assimp/lib
        )
    endif()
    target_link_libraries(${PROJECT_NAME} assimp)
endif()

# Link system frameworks (macOS)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} 
        "-framework OpenGL"
        "-framework Cocoa" 
        "-framework IOKit"
    )
endif()

# Optional: Link SDL2 if you're using it
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
    if(SDL2_FOUND)
        target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    endif()
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# Copy required DLLs to output directory on Windows
if(WIN32)
    # Copy Assimp DLL
    if(EXISTS "${CMAKE_SOURCE_DIR}/includes/assimp/build/bin/Release/assimp-vc143-mt.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/includes/assimp/build/bin/Release/assimp-vc143-mt.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()
endif()
