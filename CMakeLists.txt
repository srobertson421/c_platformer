cmake_minimum_required(VERSION 3.10)
project(ChipmunkSDLDemo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Create the main executable
add_executable(platformer main.c)

# Find SDL2
find_package(SDL2 REQUIRED)
if(NOT SDL2_FOUND)
    # Fallback to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# Find SDL2_image
find_package(SDL2_image QUIET)
if(NOT SDL2_image_FOUND)
    # Fallback to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
else()
    # Create variables that match pkg-config style
    set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARIES})
    set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIRS})
endif()

# Find Chipmunk2D
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
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

# Include directories
target_include_directories(platformer PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${CHIPMUNK_INCLUDE_DIRS}
)

# Link libraries
if(APPLE)
    # On macOS, we need to link frameworks
    target_link_libraries(platformer 
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        ${CHIPMUNK_LIBRARIES}
        m
    )
else()
    # On other platforms
    target_link_libraries(platformer 
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        ${CHIPMUNK_LIBRARIES}
        m
    )
endif()

# Add SDL2 and SDL2_image specific flags
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

# Set output name
set_target_properties(platformer PROPERTIES OUTPUT_NAME "platformer")