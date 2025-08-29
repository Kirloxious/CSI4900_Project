cmake_minimum_required(VERSION 3.21)
project(main)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(FetchContent)

find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} OpenGL::GL)

# Find or fetch GLFW
find_package(glfw3 QUIET)
if (NOT glfw3_FOUND)
    message(STATUS "GLFW not found, fetching...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4  
    )
    FetchContent_MakeAvailable(glfw)
endif()
target_link_libraries(${PROJECT_NAME} glfw)

# Find or fetch GLAD
find_package(glad CONFIG QUIET)
if (NOT glad_FOUND)
    message(STATUS "GLAD not found, fetching...")
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v2.0.8  
    )
    FetchContent_MakeAvailable(glad)
endif()
target_link_libraries(${PROJECT_NAME} glad::glad)

# Find or fetch GLM
find_package(glm QUIET)
if (NOT glm_FOUND)
    message(STATUS "GLM not found, fetching...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1 
    )
    FetchContent_MakeAvailable(glm)
endif()
target_link_libraries(${PROJECT_NAME} glm::glm)

# Link static runtime libraries (for MinGW)
if (MINGW)
    target_link_libraries(${PROJECT_NAME} -static-libgcc -static-libstdc++ -lucrt -lmsvcrt)
endif()

# Ensure required DLLs are copied next to the executable 
if (WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()

