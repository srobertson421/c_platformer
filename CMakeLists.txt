cmake_minimum_required(VERSION 3.10)
project(ChipmunkSDLDemo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Create the main executable
add_executable(platformer main.c)

# Find pkg-config first
find_package(PkgConfig)

# Find SDL2
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 sdl2)
endif()
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

# Find SDL2_image
set(SDL2_IMAGE_FOUND FALSE)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2_IMAGE SDL2_image)
endif()

if(NOT SDL2_IMAGE_FOUND)
    # Try to find SDL2_image manually
    find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
        HINTS
        ${SDL2_INCLUDE_DIRS}
        PATHS
        /usr/include/SDL2
        /usr/local/include/SDL2
        /opt/homebrew/include/SDL2
        /opt/local/include/SDL2
    )
    
    find_library(SDL2_IMAGE_LIBRARY
        NAMES SDL2_image
        PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        /opt/local/lib
    )
    
    if(SDL2_IMAGE_INCLUDE_DIR AND SDL2_IMAGE_LIBRARY)
        set(SDL2_IMAGE_FOUND TRUE)
        set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
        set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
    endif()
endif()

if(NOT SDL2_IMAGE_FOUND)
    message(FATAL_ERROR "SDL2_image not found. Please install it using your package manager.")
endif()

# Find Chipmunk2D
set(CHIPMUNK_FOUND FALSE)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CHIPMUNK chipmunk)
endif()

if(NOT CHIPMUNK_FOUND)
    # Try to find Chipmunk library directly
    find_path(CHIPMUNK_INCLUDE_DIR chipmunk/chipmunk.h
        PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        /opt/local/include
    )
    
    find_library(CHIPMUNK_LIBRARY
        NAMES chipmunk libchipmunk
        PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        /opt/local/lib
    )
    
    if(CHIPMUNK_INCLUDE_DIR AND CHIPMUNK_LIBRARY)
        set(CHIPMUNK_FOUND TRUE)
        set(CHIPMUNK_INCLUDE_DIRS ${CHIPMUNK_INCLUDE_DIR})
        set(CHIPMUNK_LIBRARIES ${CHIPMUNK_LIBRARY})
    else()
        message(FATAL_ERROR "Chipmunk2D not found. Please install it using your package manager.")
    endif()
endif()

# Debug output
message(STATUS "SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2_LIBRARIES: ${SDL2_LIBRARIES}")
message(STATUS "SDL2_IMAGE_INCLUDE_DIRS: ${SDL2_IMAGE_INCLUDE_DIRS}")
message(STATUS "SDL2_IMAGE_LIBRARIES: ${SDL2_IMAGE_LIBRARIES}")
message(STATUS "CHIPMUNK_INCLUDE_DIRS: ${CHIPMUNK_INCLUDE_DIRS}")
message(STATUS "CHIPMUNK_LIBRARIES: ${CHIPMUNK_LIBRARIES}")

# Include directories
target_include_directories(platformer PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${CHIPMUNK_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(platformer 
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${CHIPMUNK_LIBRARIES}
    m
)

# Add compile flags
if(SDL2_CFLAGS_OTHER)
    target_compile_options(platformer PRIVATE ${SDL2_CFLAGS_OTHER})
endif()
if(SDL2_IMAGE_CFLAGS_OTHER)
    target_compile_options(platformer PRIVATE ${SDL2_IMAGE_CFLAGS_OTHER})
endif()
if(CHIPMUNK_CFLAGS_OTHER)
    target_compile_options(platformer PRIVATE ${CHIPMUNK_CFLAGS_OTHER})
endif()

# Compiler flags
target_compile_options(platformer PRIVATE 
    -Wall -Wextra
)

# On Windows with MinGW, we might need to link additional libraries
if(WIN32 AND MINGW)
    target_link_libraries(platformer mingw32 SDL2main)
endif()

# On macOS, we might need to add frameworks
if(APPLE)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(COCOA_LIBRARY Cocoa)
    if(COREFOUNDATION_LIBRARY AND COCOA_LIBRARY)
        target_link_libraries(platformer ${COREFOUNDATION_LIBRARY} ${COCOA_LIBRARY})
    endif()
endif()

# Set output name
set_target_properties(platformer PROPERTIES OUTPUT_NAME "platformer")