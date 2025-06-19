#define _USE_MATH_DEFINES
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <chipmunk/chipmunk.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#ifdef __linux__
#include <execinfo.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#include <imagehlp.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define BOX_SIZE 50
#define GROUND_HEIGHT 50
#define MAX_BOXES 100

// Player movement constants
#define PLAYER_MOVE_FORCE 1500.0f
#define PLAYER_JUMP_IMPULSE 400.0f
#define MAX_HORIZONTAL_SPEED 250.0f

// Box structure to track multiple boxes
typedef struct {
    cpBody *body;
    cpShape *shape;
} Box;

// Shape type enumeration for safe debug drawing
typedef enum {
    SHAPE_TYPE_SEGMENT,
    SHAPE_TYPE_POLYGON
} ShapeType;

// Extended shape structure for debug drawing
typedef struct {
    cpShape *shape;
    ShapeType type;
} DebugShape;

// Forward declarations for cross-platform error handling
void show_error_message(const char* title, const char* message);
void safe_log_write(FILE* file, const char* format, ...);

#ifdef _WIN32
void print_windows_stack_trace_from_context(CONTEXT* context, FILE *output_file);
#endif

// Thread-safe logging with immediate flush
void safe_log_write(FILE* file, const char* format, ...) {
    if (!file) return;
    
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    
    fflush(file);  // Critical: ensure data is written immediately
}

// Show error message box that works even if SDL isn't initialized
void show_error_message(const char* title, const char* message) {
#ifdef _WIN32
    // Try SDL message box first (works even before SDL_Init in most cases)
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL) < 0) {
        // Fallback to Windows MessageBox if SDL fails
        MessageBoxA(NULL, message, title, MB_OK | MB_ICONERROR | MB_TOPMOST);
    }
#else
    // For Linux/macOS, try SDL message box (may not work without display)
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL) < 0) {
        // Fallback: print to stderr if SDL fails
        fprintf(stderr, "\n=== %s ===\n%s\n=== END MESSAGE ===\n", title, message);
        fflush(stderr);
    }
#endif
}

#ifdef _WIN32
// Windows unhandled exception filter for catching crashes that don't trigger signals
LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* exc_info) {
    FILE *crash_file = NULL;
    time_t now;
    char time_str[64];
    
    // Get current time for timestamp
    time(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Try to open crash log file
    crash_file = fopen("crash.log", "a");
    
    // Show immediate alert to user
    char alert_msg[512];
    snprintf(alert_msg, sizeof(alert_msg), 
             "Application crashed with Windows exception 0x%08lx.\n\nCrash details saved to crash.log\n\nTime: %s", 
             exc_info->ExceptionRecord->ExceptionCode, time_str);
    show_error_message("Application Crash Detected", alert_msg);
    
    // Write to stderr (for console if available)
    fprintf(stderr, "\n=== WINDOWS EXCEPTION DETECTED ===\n");
    fprintf(stderr, "Time: %s\n", time_str);
    fprintf(stderr, "Exception Code: 0x%08lx\n", exc_info->ExceptionRecord->ExceptionCode);
    fprintf(stderr, "Exception Address: 0x%p\n", exc_info->ExceptionRecord->ExceptionAddress);
    fflush(stderr);
    
    // Write to file if we could open it
    if (crash_file) {
        safe_log_write(crash_file, "\n=== WINDOWS EXCEPTION DETECTED ===\n");
        safe_log_write(crash_file, "Time: %s\n", time_str);
        safe_log_write(crash_file, "Exception Code: 0x%08lx\n", exc_info->ExceptionRecord->ExceptionCode);
        safe_log_write(crash_file, "Exception Address: 0x%p\n", exc_info->ExceptionRecord->ExceptionAddress);
    }
    
    // Print exception type
    const char* exception_name = "Unknown";
    switch (exc_info->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            exception_name = "Access Violation";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            exception_name = "Stack Overflow";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            exception_name = "Divide by Zero";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            exception_name = "Illegal Instruction";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            exception_name = "Float Divide by Zero";
            break;
        default:
            break;
    }
    
    fprintf(stderr, "Exception Type: %s\n", exception_name);
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "Exception Type: %s\n", exception_name);
    }
    
    // Get stack trace using the exception context
    fprintf(stderr, "Stack trace (Windows Exception):\n");
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "Stack trace (Windows Exception):\n");
    }
    
    print_windows_stack_trace_from_context(exc_info->ContextRecord, crash_file);
    
    fprintf(stderr, "=== END EXCEPTION INFO ===\n");
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "=== END EXCEPTION INFO ===\n\n");
        fflush(crash_file);  // Extra safety flush
        fclose(crash_file);
    }
    
    // Return to let Windows handle the exception (will terminate the process)
    return EXCEPTION_EXECUTE_HANDLER;
}

// Windows-specific stack trace function using provided context
void print_windows_stack_trace_from_context(CONTEXT* context, FILE *output_file) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    
    // Initialize symbol handler
    SymInitialize(process, NULL, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    
    // Setup stack frame for walking
    STACKFRAME64 frame;
    memset(&frame, 0, sizeof(STACKFRAME64));
    
    #ifdef _M_X64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Rsp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    #else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    #endif
    
    // Walk the stack
    int frame_count = 0;
    while (frame_count < 10 && 
           StackWalk64(machineType, process, thread, &frame, context, 
                      NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        
        if (frame.AddrPC.Offset == 0) break;
        
        // Get symbol information
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        
        DWORD64 displacement = 0;
        char symbol_name[256] = "Unknown";
        
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
            strncpy(symbol_name, symbol->Name, sizeof(symbol_name) - 1);
            symbol_name[sizeof(symbol_name) - 1] = '\0';
        }
        
        // Get line information
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD line_displacement = 0;
        char file_info[512] = "";
        
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line)) {
            snprintf(file_info, sizeof(file_info), " [%s:%lu]", line.FileName, line.LineNumber);
        }
        
        // Print frame information
        fprintf(stderr, "  #%d: 0x%llx %s%s\n", frame_count, 
                (unsigned long long)frame.AddrPC.Offset, symbol_name, file_info);
        
        if (output_file) {
            fprintf(output_file, "  #%d: 0x%llx %s%s\n", frame_count, 
                    (unsigned long long)frame.AddrPC.Offset, symbol_name, file_info);
        }
        
        frame_count++;
    }
    
    SymCleanup(process);
}

// Windows-specific stack trace function
void print_windows_stack_trace(FILE *output_file) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    
    // Initialize symbol handler
    SymInitialize(process, NULL, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    
    // Get the current context
    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);
    
    // Setup stack frame for walking
    STACKFRAME64 frame;
    memset(&frame, 0, sizeof(STACKFRAME64));
    
    #ifdef _M_X64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rsp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    #else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    #endif
    
    // Walk the stack
    int frame_count = 0;
    while (frame_count < 10 && 
           StackWalk64(machineType, process, thread, &frame, &context, 
                      NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        
        if (frame.AddrPC.Offset == 0) break;
        
        // Get symbol information
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        
        DWORD64 displacement = 0;
        char symbol_name[256] = "Unknown";
        
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
            strncpy(symbol_name, symbol->Name, sizeof(symbol_name) - 1);
            symbol_name[sizeof(symbol_name) - 1] = '\0';
        }
        
        // Get line information
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD line_displacement = 0;
        char file_info[512] = "";
        
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line)) {
            snprintf(file_info, sizeof(file_info), " [%s:%lu]", line.FileName, line.LineNumber);
        }
        
        // Print frame information
        fprintf(stderr, "  #%d: 0x%llx %s%s\n", frame_count, 
                (unsigned long long)frame.AddrPC.Offset, symbol_name, file_info);
        
        if (output_file) {
            fprintf(output_file, "  #%d: 0x%llx %s%s\n", frame_count, 
                    (unsigned long long)frame.AddrPC.Offset, symbol_name, file_info);
        }
        
        frame_count++;
    }
    
    SymCleanup(process);
}
#endif

// Crash handler function
void crash_handler(int sig) {
    FILE *crash_file = NULL;
    time_t now;
    char time_str[64];
    
    // Get current time for timestamp
    time(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Try to open crash log file
    crash_file = fopen("crash.log", "a");
    
    // Show immediate alert to user
    char alert_msg[512];
    const char* signal_name = "Unknown";
    switch (sig) {
        case SIGSEGV: signal_name = "Segmentation Fault"; break;
        case SIGABRT: signal_name = "Abort"; break;
        case SIGFPE: signal_name = "Floating Point Exception"; break;
        case SIGILL: signal_name = "Illegal Instruction"; break;
        default: break;
    }
    
    snprintf(alert_msg, sizeof(alert_msg), 
             "Application crashed with signal %d (%s).\n\nCrash details saved to crash.log\n\nTime: %s", 
             sig, signal_name, time_str);
    
#ifdef _WIN32
    show_error_message("Application Crash Detected", alert_msg);
#else
    // For Linux/macOS, print to stderr (console available)
    fprintf(stderr, "\n=== CRASH ALERT ===\n%s\n=== END ALERT ===\n", alert_msg);
    fflush(stderr);
#endif
    
    // Write to stderr (for console if available)
    fprintf(stderr, "\n=== CRASH DETECTED ===\n");
    fprintf(stderr, "Time: %s\n", time_str);
    fprintf(stderr, "Signal: %d (%s)\n", sig, signal_name);
    fflush(stderr);
    
    // Write to file if we could open it
    if (crash_file) {
        safe_log_write(crash_file, "\n=== CRASH DETECTED ===\n");
        safe_log_write(crash_file, "Time: %s\n", time_str);
        safe_log_write(crash_file, "Signal: %d (%s)\n", sig, signal_name);
    }
    
#ifdef __linux__
    // Get stack trace (Linux)
    void *array[10];
    size_t size;
    char **strings;
    size_t i;
    
    size = backtrace(array, 10);
    strings = backtrace_symbols(array, size);
    
    fprintf(stderr, "Stack trace (%zd frames):\n", size);
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "Stack trace (%zd frames):\n", size);
    }
    
    for (i = 0; i < size; i++) {
        fprintf(stderr, "  %s\n", strings[i]);
        if (crash_file) {
            safe_log_write(crash_file, "  %s\n", strings[i]);
        }
    }
    
    free(strings);
#elif defined(_WIN32)
    // Get stack trace (Windows)
    fprintf(stderr, "Stack trace (Windows):\n");
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "Stack trace (Windows):\n");
    }
    
    print_windows_stack_trace(crash_file);
#else
    // No stack trace available on this platform
    fprintf(stderr, "Stack trace not available on this platform\n");
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "Stack trace not available on this platform\n");
    }
#endif
    
    fprintf(stderr, "=== END CRASH INFO ===\n");
    fflush(stderr);
    if (crash_file) {
        safe_log_write(crash_file, "=== END CRASH INFO ===\n\n");
        fflush(crash_file);  // Extra safety flush
        fclose(crash_file);
    }
    
    // Re-raise the signal to get core dump
    signal(sig, SIG_DFL);
    raise(sig);
}

// Test crash function for debugging crash handlers
void test_crash_handlers() {
    FILE *test_file = fopen("crash.log", "a");
    if (test_file) {
        time_t now;
        char time_str[64];
        time(&now);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        safe_log_write(test_file, "\n=== MANUAL CRASH TEST INITIATED ===\n");
        safe_log_write(test_file, "Time: %s\n", time_str);
        safe_log_write(test_file, "Testing crash handlers...\n");
        fflush(test_file);
        fclose(test_file);
    }
    
    // Show alert before crash
    show_error_message("Crash Test", "About to trigger a segmentation fault to test crash handlers.");
    
    // Trigger segmentation fault
    int *p = NULL;
    *p = 42; // This should crash the program
}

// Setup crash handlers
void setup_crash_handlers() {
    signal(SIGSEGV, crash_handler);  // Segmentation fault
    signal(SIGABRT, crash_handler);  // Abort
    signal(SIGFPE, crash_handler);   // Floating point exception
    signal(SIGILL, crash_handler);   // Illegal instruction
    
#ifdef _WIN32
    // Also set up Windows structured exception handling
    SetUnhandledExceptionFilter(windows_exception_handler);
    
    // Additional Windows signal handlers (some crashes may still trigger signals)
    signal(SIGINT, crash_handler);   // Interrupt
    signal(SIGTERM, crash_handler);  // Termination
#endif
    
    // Write a test log entry to verify crash logging is working
    FILE *test_file = fopen("crash.log", "a");
    if (test_file) {
        time_t now;
        char time_str[64];
        time(&now);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        safe_log_write(test_file, "\n=== CRASH HANDLER INITIALIZED ===\n");
        safe_log_write(test_file, "Time: %s\n", time_str);
        safe_log_write(test_file, "Platform: ");
#ifdef _WIN32
        safe_log_write(test_file, "Windows\n");
#elif defined(__APPLE__)
        safe_log_write(test_file, "macOS\n");
#elif defined(__linux__)
        safe_log_write(test_file, "Linux\n");
#else
        safe_log_write(test_file, "Unknown\n");
#endif
        safe_log_write(test_file, "Crash logging system is active\n");
        safe_log_write(test_file, "=== END INIT INFO ===\n\n");
        fflush(test_file);
        fclose(test_file);
    }
}

// Sprite frame structure
typedef struct {
    int x, y;        // Position in spritesheet
    int width, height; // Frame dimensions
} SpriteFrame;

// Animation structure
typedef struct {
    SpriteFrame *frames;  // Array of frames
    int frameCount;      // Number of frames
    float frameTime;     // Time per frame in seconds
    bool loop;          // Whether animation loops
} Animation;

// Sprite structure
typedef struct {
    SDL_Texture *texture;     // The spritesheet texture
    Animation *animations;    // Array of animations
    int animationCount;      // Number of animations
    int currentAnimation;    // Current playing animation
    int currentFrame;        // Current frame in animation
    float animationTimer;    // Timer for frame changes
    bool isPlaying;         // Whether animation is playing
    bool facingLeft;        // Whether sprite should be flipped horizontally
} Sprite;

// Animation states
typedef enum {
    ANIM_IDLE = 0,
    ANIM_WALK,
    ANIM_JUMP,
    ANIM_COUNT
} AnimationState;

// Convert Chipmunk coordinates to SDL coordinates
void cpToSDL(cpVect pos, int *x, int *y) {
    *x = (int)pos.x;
    *y = WINDOW_HEIGHT - (int)pos.y;
}

// Convert SDL coordinates to Chipmunk coordinates
cpVect sdlToCP(int x, int y) {
    return cpv(x, WINDOW_HEIGHT - y);
}

// Load character spritesheet from file
SDL_Texture* loadCharacterSpritesheet(SDL_Renderer *renderer) {
    SDL_Texture *texture = IMG_LoadTexture(renderer, "./assets/characters.png");
    if (!texture) {
        fprintf(stderr, "Failed to load character spritesheet: %s\n", IMG_GetError());
        return NULL;
    }
    return texture;
}

// Initialize sprite with character spritesheet
Sprite createCharacterSprite(SDL_Renderer *renderer) {
    Sprite sprite = {0};
    
    // Load character spritesheet
    sprite.texture = loadCharacterSpritesheet(renderer);
    if (!sprite.texture) {
        return sprite;
    }
    
    // Allocate animations
    sprite.animationCount = ANIM_COUNT;
    sprite.animations = malloc(sizeof(Animation) * ANIM_COUNT);
    if (!sprite.animations) {
        fprintf(stderr, "Failed to allocate memory for animations\n");
        SDL_DestroyTexture(sprite.texture);
        sprite.texture = NULL;
        return sprite;
    }
    
    // Setup idle animation - frame (1,0) = x:0, y:32, 32x32 (swapped x,y)
    sprite.animations[ANIM_IDLE].frameCount = 1;
    sprite.animations[ANIM_IDLE].frames = malloc(sizeof(SpriteFrame));
    sprite.animations[ANIM_IDLE].frames[0] = (SpriteFrame){0, 32, 32, 32};
    sprite.animations[ANIM_IDLE].frameTime = 1.0f;
    sprite.animations[ANIM_IDLE].loop = true;
    
    // Setup walk animation - frames (1,0), (1,1), (1,2), (1,3) with swapped coordinates
    sprite.animations[ANIM_WALK].frameCount = 4;
    sprite.animations[ANIM_WALK].frames = malloc(sizeof(SpriteFrame) * 4);
    sprite.animations[ANIM_WALK].frames[0] = (SpriteFrame){0, 32, 32, 32};   // (1,0) -> (0,1)
    sprite.animations[ANIM_WALK].frames[1] = (SpriteFrame){32, 32, 32, 32};  // (1,1) -> (1,1)
    sprite.animations[ANIM_WALK].frames[2] = (SpriteFrame){64, 32, 32, 32};  // (1,2) -> (2,1)
    sprite.animations[ANIM_WALK].frames[3] = (SpriteFrame){96, 32, 32, 32};  // (1,3) -> (3,1)
    sprite.animations[ANIM_WALK].frameTime = 0.15f; // Slightly faster animation
    sprite.animations[ANIM_WALK].loop = true;
    
    // Setup jump animation - use frame (1,0) for now
    sprite.animations[ANIM_JUMP].frameCount = 1;
    sprite.animations[ANIM_JUMP].frames = malloc(sizeof(SpriteFrame));
    sprite.animations[ANIM_JUMP].frames[0] = (SpriteFrame){0, 32, 32, 32};
    sprite.animations[ANIM_JUMP].frameTime = 1.0f;
    sprite.animations[ANIM_JUMP].loop = false;
    
    // Start with idle animation, facing right
    sprite.currentAnimation = ANIM_IDLE;
    sprite.currentFrame = 0;
    sprite.animationTimer = 0.0f;
    sprite.isPlaying = true;
    sprite.facingLeft = false;
    
    return sprite;
}

// Update sprite animation
void updateSprite(Sprite *sprite, float deltaTime) {
    if (!sprite->isPlaying || sprite->animationCount == 0) return;
    
    Animation *anim = &sprite->animations[sprite->currentAnimation];
    
    sprite->animationTimer += deltaTime;
    
    if (sprite->animationTimer >= anim->frameTime) {
        sprite->animationTimer = 0.0f;
        sprite->currentFrame++;
        
        if (sprite->currentFrame >= anim->frameCount) {
            if (anim->loop) {
                sprite->currentFrame = 0;
            } else {
                sprite->currentFrame = anim->frameCount - 1;
                sprite->isPlaying = false;
            }
        }
    }
}

// Set sprite animation
void setSpriteAnimation(Sprite *sprite, int animation) {
    if (animation >= 0 && animation < sprite->animationCount && 
        animation != sprite->currentAnimation) {
        sprite->currentAnimation = animation;
        sprite->currentFrame = 0;
        sprite->animationTimer = 0.0f;
        sprite->isPlaying = true;
    }
}

// Render sprite at given position with optional horizontal flipping
void renderSprite(SDL_Renderer *renderer, Sprite *sprite, int x, int y) {
    if (!sprite->texture || sprite->animationCount == 0) {
        return;
    }
    
    Animation *anim = &sprite->animations[sprite->currentAnimation];
    if (sprite->currentFrame >= anim->frameCount) {
        return;
    }
    
    SpriteFrame *frame = &anim->frames[sprite->currentFrame];
    
    SDL_Rect srcRect = {
        frame->x, frame->y,
        frame->width, frame->height
    };
    
    // Scale sprite to double the physics body size (32x32 -> 100x100)
    int spriteSize = BOX_SIZE * 2;  // Double the physics body size
    SDL_Rect dstRect = {
        x - spriteSize/2,                    // Center horizontally
        y - spriteSize + BOX_SIZE/2,         // Align physics body with bottom of sprite
        spriteSize, spriteSize               // Double scale (100x100)
    };
    
    // Use SDL_RenderCopyEx for flipping support
    SDL_RendererFlip flip = sprite->facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, sprite->texture, &srcRect, &dstRect, 0.0, NULL, flip);
}

// Cleanup sprite resources
void destroySprite(Sprite *sprite) {
    if (sprite->texture) {
        SDL_DestroyTexture(sprite->texture);
    }
    
    for (int i = 0; i < sprite->animationCount; i++) {
        if (sprite->animations[i].frames) {
            free(sprite->animations[i].frames);
        }
    }
    
    if (sprite->animations) {
        free(sprite->animations);
    }
}

// Function to create a new box at given position
Box createBox(cpSpace *space, cpVect position) {
    cpFloat mass = 1.0f;
    cpFloat moment = cpMomentForBox(mass, BOX_SIZE, BOX_SIZE);
    cpBody *body = cpSpaceAddBody(space, cpBodyNew(mass, moment));
    cpBodySetPosition(body, position);
    
    cpShape *shape = cpSpaceAddShape(space, 
        cpBoxShapeNew(body, BOX_SIZE, BOX_SIZE, 0.0f));
    cpShapeSetFriction(shape, 0.4f);
    
    Box box = {body, shape};
    return box;
}

// Check if player is on any surface (ground or other boxes) using collision detection
bool isOnGround(cpSpace *space, cpBody *body, cpShape *playerShape) {
    cpVect pos = cpBodyGetPosition(body);
    cpVect vel = cpBodyGetVelocity(body);
    
    // Don't allow jumping if moving upward quickly
    if (vel.y > 10.0f) {
        return false;
    }
    
    // Cast a short ray downward from the bottom of the player box
    cpVect start = cpv(pos.x, pos.y - BOX_SIZE/2);
    cpVect end = cpv(pos.x, pos.y - BOX_SIZE/2 - 10.0f); // Check 10 pixels below
    
    // Create a shape filter that excludes the player's own shape
    cpShapeFilter filter = {
        .group = CP_NO_GROUP,
        .categories = CP_ALL_CATEGORIES,
        .mask = CP_ALL_CATEGORIES
    };
    
    // Perform ray cast to detect any surface below (excluding player's own shape)
    cpSegmentQueryInfo info;
    cpShape *hitShape = cpSpaceSegmentQueryFirst(space, start, end, 0.0f, filter, &info);
    
    // Make sure we didn't hit the player's own shape
    if (hitShape == playerShape) {
        hitShape = NULL;
    }
    
    // Also check if we're very close to the static ground level
    bool nearGround = (pos.y <= GROUND_HEIGHT + BOX_SIZE/2 + 5.0f);
    
    return (hitShape != NULL) || nearGround;
}

// Apply player movement forces
void updatePlayerMovement(cpSpace *space, cpBody *playerBody, cpShape *playerShape, bool left, bool right, bool jump) {
    cpVect vel = cpBodyGetVelocity(playerBody);
    cpVect pos = cpBodyGetPosition(playerBody);
    
    // Limit rotation to prevent coordinate system flipping
    cpFloat angVel = cpBodyGetAngularVelocity(playerBody);
    if (fabs(angVel) > 2.0f) {
        cpBodySetAngularVelocity(playerBody, angVel * 0.5f); // Dampen rotation
    }
    
    // Keep player mostly upright
    cpFloat angle = cpBodyGetAngle(playerBody);
    if (fabs(angle) > M_PI/6) { // If rotated more than 30 degrees
        cpBodySetAngle(playerBody, angle * 0.9f); // Gradually return to upright
    }
    
    // Horizontal movement - use WORLD coordinates, not local
    if (left && vel.x > -MAX_HORIZONTAL_SPEED) {
        cpBodyApplyForceAtWorldPoint(playerBody, cpv(-PLAYER_MOVE_FORCE, 0), pos);
    }
    if (right && vel.x < MAX_HORIZONTAL_SPEED) {
        cpBodyApplyForceAtWorldPoint(playerBody, cpv(PLAYER_MOVE_FORCE, 0), pos);
    }
    
    // Apply horizontal damping for better control
    if (!left && !right) {
        cpFloat damping = 0.8f;
        cpBodySetVelocity(playerBody, cpv(vel.x * damping, vel.y));
    }
    
    // Jumping - use WORLD coordinates
    if (jump && isOnGround(space, playerBody, playerShape)) {
        cpBodyApplyImpulseAtWorldPoint(playerBody, cpv(0, PLAYER_JUMP_IMPULSE), pos);
    }
}

// Safe function to draw debug physics outlines using bounding boxes
void drawDebugShape(SDL_Renderer *renderer, cpShape *shape, ShapeType type) {
    cpBody *body = cpShapeGetBody(shape);
    cpBB bb = cpShapeGetBB(shape);
    
    // Convert bounding box to SDL coordinates
    int x1, y1, x2, y2;
    cpToSDL(cpv(bb.l, bb.b), &x1, &y1);
    cpToSDL(cpv(bb.r, bb.t), &x2, &y2);
    
    // Set color based on body type
    if (cpBodyGetType(body) == CP_BODY_TYPE_DYNAMIC) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow for dynamic
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green for static
    }
    
    // Draw bounding box
    SDL_Rect rect = {x1, y2, x2 - x1, y1 - y2};
    SDL_RenderDrawRect(renderer, &rect);
    
    // For polygon shapes, try to draw the actual vertices safely
    if (type == SHAPE_TYPE_POLYGON) {
        int count = cpPolyShapeGetCount(shape);
        if (count > 0 && count <= 10) { // Safety check
            SDL_Point points[count + 1];
            
            for (int i = 0; i < count; i++) {
                cpVect v = cpBodyLocalToWorld(body, cpPolyShapeGetVert(shape, i));
                cpToSDL(v, &points[i].x, &points[i].y);
            }
            // Close the polygon
            points[count] = points[0];
            
            SDL_RenderDrawLines(renderer, points, count + 1);
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // Setup crash handlers
    setup_crash_handlers();
    
    // Log application start
    FILE *log_file = fopen("crash.log", "a");
    if (log_file) {
        time_t now;
        char time_str[64];
        time(&now);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        safe_log_write(log_file, "\n=== APPLICATION STARTED ===\n");
        safe_log_write(log_file, "Time: %s\n", time_str);
        safe_log_write(log_file, "Crash handlers are active. Press F9 to test.\n");
        safe_log_write(log_file, "=== END START LOG ===\n\n");
        fflush(log_file);
        fclose(log_file);
    }
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        fprintf(stderr, "SDL_image initialization failed: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow(
        "Chipmunk2D Box Collision Demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create Chipmunk space
    cpSpace *space = cpSpaceNew();
    if (!space) {
        fprintf(stderr, "Failed to create Chipmunk space\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    cpSpaceSetGravity(space, cpv(0, -980));

    // Create static ground body
    cpBody *groundBody = cpSpaceGetStaticBody(space);
    cpShape *ground = cpSegmentShapeNew(groundBody, 
        cpv(0, GROUND_HEIGHT), 
        cpv(WINDOW_WIDTH, GROUND_HEIGHT), 
        0.0f);
    cpShapeSetFriction(ground, 0.3f);
    cpSpaceAddShape(space, ground);

    // Initialize box array
    Box boxes[MAX_BOXES];
    int boxCount = 0;
    
    // Debug visualization toggle
    bool showDebug = false;
    
    // Player input state
    bool leftPressed = false;
    bool rightPressed = false;
    bool jumpPressed = false;
    
    // Create initial box (player)
    boxes[boxCount] = createBox(space, cpv(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 50));
    cpBody *playerBody = boxes[0].body;
    cpShape *playerShape = boxes[0].shape;
    boxCount++;
    
    // Create player sprite
    Sprite playerSprite = createCharacterSprite(renderer);
    if (!playerSprite.texture) {
        fprintf(stderr, "Failed to load player sprite\n");
        // Continue without sprite
    }

    // Setup input
    SDL_RaiseWindow(window);
    SDL_SetWindowInputFocus(window);
    SDL_StartTextInput();
    SDL_PumpEvents();
    
    // Main loop
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();
    int frameCount = 0;
    
    while (running) {
        frameCount++;
        
        Uint32 currentTime = SDL_GetTicks();
        cpFloat dt = (currentTime - lastTime) / 1000.0f;
        
        // Handle first frame case where dt would be 0
        if (dt <= 0.0f) {
            dt = 0.016f; // Default to 60 FPS
        }
        
        lastTime = currentTime;

        // Check keyboard state directly - frequent polling for responsive input
        if (frameCount % 5 == 0) { // Every ~0.08 seconds (5 frames)
            const Uint8* keystate = SDL_GetKeyboardState(NULL);
            
            // Update input state based on direct keyboard polling
            leftPressed = (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT]) ? true : false;
            rightPressed = (keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) ? true : false;
            jumpPressed = (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_SPACE]) ? true : false;
        }
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT && boxCount < MAX_BOXES) {
                    if (event.button.x >= 0 && event.button.x < WINDOW_WIDTH && 
                        event.button.y >= 0 && event.button.y < WINDOW_HEIGHT) {
                        cpVect mousePos = sdlToCP(event.button.x, event.button.y);
                        boxes[boxCount] = createBox(space, mousePos);
                        boxCount++;
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_F1:
                        showDebug = !showDebug;
                        printf("Debug visualization: %s\n", showDebug ? "ON" : "OFF");
                        break;
                    case SDLK_F9:
                        // Test crash handlers (F9 key)
                        test_crash_handlers();
                        break;
                    case SDLK_a:
                    case SDLK_LEFT:
                        leftPressed = true;
                        break;
                    case SDLK_d:
                    case SDLK_RIGHT:
                        rightPressed = true;
                        break;
                    case SDLK_w:
                    case SDLK_UP:
                    case SDLK_SPACE:
                        jumpPressed = true;
                        break;
                }
            } else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_a:
                    case SDLK_LEFT:
                        leftPressed = false;
                        break;
                    case SDLK_d:
                    case SDLK_RIGHT:
                        rightPressed = false;
                        break;
                    case SDLK_w:
                    case SDLK_UP:
                    case SDLK_SPACE:
                        jumpPressed = false;
                        break;
                }
            }
        }
        
        // Update player movement
        updatePlayerMovement(space, playerBody, playerShape, leftPressed, rightPressed, jumpPressed);
        
        // Update player sprite animation based on movement state
        if (playerSprite.texture) {
            cpVect vel = cpBodyGetVelocity(playerBody);
            bool onGround = isOnGround(space, playerBody, playerShape);
            
            // Update sprite direction based on velocity
            if (vel.x < -5.0f) {
                playerSprite.facingLeft = true;
            } else if (vel.x > 5.0f) {
                playerSprite.facingLeft = false;
            }
            
            // Update animation based on state
            if (!onGround) {
                setSpriteAnimation(&playerSprite, ANIM_JUMP);
            } else if (fabs(vel.x) > 10.0f) {
                setSpriteAnimation(&playerSprite, ANIM_WALK);
            } else {
                setSpriteAnimation(&playerSprite, ANIM_IDLE);
            }
            
            // Update sprite animation timer
            updateSprite(&playerSprite, dt);
        }
        
        // Update physics
        if (dt > 0.033f) {
            dt = 0.033f; // Clamp to reasonable value
        }
        
        cpSpaceStep(space, dt);

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw ground
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect groundRect = {
            0, 
            WINDOW_HEIGHT - GROUND_HEIGHT, 
            WINDOW_WIDTH, 
            GROUND_HEIGHT
        };
        SDL_RenderFillRect(renderer, &groundRect);

        // Draw all boxes
        for (int i = 0; i < boxCount; i++) {
            cpVect pos = cpBodyGetPosition(boxes[i].body);
            cpFloat angle = cpBodyGetAngle(boxes[i].body);
            
            int x, y;
            cpToSDL(pos, &x, &y);
            
            if (i == 0) {
                // Draw player as animated sprite
                if (playerSprite.texture) {
                    renderSprite(renderer, &playerSprite, x, y);
                } else {
                    // Fallback to rectangle if sprite failed to load
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                    SDL_Rect boxRect = {
                        x - BOX_SIZE/2,
                        y - BOX_SIZE/2,
                        BOX_SIZE,
                        BOX_SIZE
                    };
                    SDL_RenderFillRect(renderer, &boxRect);
                }
            } else {
                // Draw other boxes as rectangles
                SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
                
                // Simple rotation rendering (for visual feedback)
                if (fabs(angle) > 0.01) {
                    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
                }
                
                SDL_Rect boxRect = {
                    x - BOX_SIZE/2,
                    y - BOX_SIZE/2,
                    BOX_SIZE,
                    BOX_SIZE
                };
                
                SDL_RenderFillRect(renderer, &boxRect);
            }
        }
        
        // Draw debug visualization if enabled
        if (showDebug) {
            // Draw ground debug outline
            drawDebugShape(renderer, ground, SHAPE_TYPE_SEGMENT);
            
            // Draw all box debug outlines
            for (int i = 0; i < boxCount; i++) {
                drawDebugShape(renderer, boxes[i].shape, SHAPE_TYPE_POLYGON);
            }
        }

        // Present
        SDL_RenderPresent(renderer);

        // Cap framerate
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    destroySprite(&playerSprite);
    for (int i = 0; i < boxCount; i++) {
        cpShapeFree(boxes[i].shape);
        cpBodyFree(boxes[i].body);
    }
    cpShapeFree(ground);
    cpSpaceFree(space);
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    
    // Log normal application exit
    log_file = fopen("crash.log", "a");
    if (log_file) {
        time_t now;
        char time_str[64];
        time(&now);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        safe_log_write(log_file, "\n=== APPLICATION EXITED NORMALLY ===\n");
        safe_log_write(log_file, "Time: %s\n", time_str);
        safe_log_write(log_file, "=== END EXIT LOG ===\n\n");
        fflush(log_file);
        fclose(log_file);
    }

    return 0;
}