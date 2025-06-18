cmake_minimum_required(VERSION 3.10)
project(ChipmunkSDLDemo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# Find Chipmunk2D
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CHIPMUNK chipmunk)
endif()

if(NOT CHIPMUNK_FOUND)
    # Fallback to direct linking
    find_library(CHIPMUNK_LIBRARY chipmunk)
    if(CHIPMUNK_LIBRARY)
        set(CHIPMUNK_LIBRARIES ${CHIPMUNK_LIBRARY})
        set(CHIPMUNK_INCLUDE_DIRS "/usr/include")
    else()
        message(FATAL_ERROR "Chipmunk2D library not found")
    endif()
endif()

# Add executable
add_executable(physics_demo main.c)

# Include directories
target_include_directories(physics_demo PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${CHIPMUNK_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(physics_demo 
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${CHIPMUNK_LIBRARIES}
    m
)

# Compiler flags
target_compile_options(physics_demo PRIVATE 
    -Wall -Wextra
)
if(CHIPMUNK_CFLAGS_OTHER)
    target_compile_options(physics_demo PRIVATE ${CHIPMUNK_CFLAGS_OTHER})
endif()