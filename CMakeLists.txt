cmake_minimum_required(VERSION 3.10)
project(ChipmunkSDLDemo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find pkg-config first - required for most libraries
find_package(PkgConfig REQUIRED)

# Find SDL2
pkg_check_modules(SDL2 REQUIRED sdl2)

# Find SDL2_image
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

# Find Chipmunk2D
pkg_check_modules(CHIPMUNK chipmunk)

if(NOT CHIPMUNK_FOUND)
    # Try to find Chipmunk library directly
    find_path(CHIPMUNK_INCLUDE_DIR chipmunk/chipmunk.h
        PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        /opt/local/include
        ${CMAKE_PREFIX_PATH}/include
    )
    
    find_library(CHIPMUNK_LIBRARY
        NAMES chipmunk libchipmunk
        PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        /opt/local/lib
        ${CMAKE_PREFIX_PATH}/lib
    )
    
    if(CHIPMUNK_INCLUDE_DIR AND CHIPMUNK_LIBRARY)
        set(CHIPMUNK_FOUND TRUE)
        set(CHIPMUNK_INCLUDE_DIRS ${CHIPMUNK_INCLUDE_DIR})
        set(CHIPMUNK_LIBRARIES ${CHIPMUNK_LIBRARY})
        message(STATUS "Found Chipmunk: ${CHIPMUNK_LIBRARY}")
    else()
        message(FATAL_ERROR "Chipmunk2D not found. Please install it using your package manager.")
    endif()
endif()

# Debug output
message(STATUS "SDL2_FOUND: ${SDL2_FOUND}")
message(STATUS "SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2_LIBRARY_DIRS: ${SDL2_LIBRARY_DIRS}")
message(STATUS "SDL2_LIBRARIES: ${SDL2_LIBRARIES}")
message(STATUS "SDL2_LDFLAGS: ${SDL2_LDFLAGS}")
message(STATUS "SDL2_IMAGE_FOUND: ${SDL2_IMAGE_FOUND}")
message(STATUS "SDL2_IMAGE_INCLUDE_DIRS: ${SDL2_IMAGE_INCLUDE_DIRS}")
message(STATUS "SDL2_IMAGE_LIBRARY_DIRS: ${SDL2_IMAGE_LIBRARY_DIRS}")
message(STATUS "SDL2_IMAGE_LIBRARIES: ${SDL2_IMAGE_LIBRARIES}")
message(STATUS "SDL2_IMAGE_LDFLAGS: ${SDL2_IMAGE_LDFLAGS}")
message(STATUS "CHIPMUNK_INCLUDE_DIRS: ${CHIPMUNK_INCLUDE_DIRS}")
message(STATUS "CHIPMUNK_LIBRARIES: ${CHIPMUNK_LIBRARIES}")

# Create the main executable
add_executable(platformer main.c logging.c)

# Include directories
target_include_directories(platformer PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${CHIPMUNK_INCLUDE_DIRS}
)

# Link directories - needed for pkg-config libraries
target_link_directories(platformer PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_IMAGE_LIBRARY_DIRS}
    ${CHIPMUNK_LIBRARY_DIRS}
)

# On Windows with MinGW, we need special handling
if(WIN32 AND MINGW)
    # MinGW requires specific library order and debugging libraries
    target_link_libraries(platformer 
        mingw32
        ${SDL2_LDFLAGS}
        ${SDL2_IMAGE_LDFLAGS}
        ${CHIPMUNK_LIBRARIES}
        dbghelp
        imagehlp
        m
    )
elseif(WIN32)
    # Other Windows compilers
    target_link_libraries(platformer 
        ${SDL2_LDFLAGS}
        ${SDL2_IMAGE_LDFLAGS}
        ${CHIPMUNK_LIBRARIES}
        dbghelp
        imagehlp
        m
    )
else()
    # Other platforms
    target_link_libraries(platformer 
        ${SDL2_LDFLAGS}
        ${SDL2_IMAGE_LDFLAGS}
        ${CHIPMUNK_LIBRARIES}
        m
    )
endif()

# Add compile flags
target_compile_options(platformer PRIVATE 
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_IMAGE_CFLAGS_OTHER}
    ${CHIPMUNK_CFLAGS_OTHER}
    -Wall -Wextra
)

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