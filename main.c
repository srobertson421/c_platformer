#define _USE_MATH_DEFINES
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <chipmunk/chipmunk.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "logging.h"

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
    LOG_DEBUG("Attempting to load character spritesheet from ./assets/characters.png");
    SDL_Texture *texture = IMG_LoadTexture(renderer, "./assets/characters.png");
    if (!texture) {
        LOG_ERROR("Failed to load character spritesheet: %s", IMG_GetError());
        printf("Failed to load character spritesheet: %s\n", IMG_GetError());
        return NULL;
    }
    LOG_DEBUG("Character spritesheet loaded successfully");
    return texture;
}

// Initialize sprite with character spritesheet
Sprite createCharacterSprite(SDL_Renderer *renderer) {
    LOG_DEBUG("Creating character sprite...");
    Sprite sprite = {0};
    
    // Load character spritesheet
    sprite.texture = loadCharacterSpritesheet(renderer);
    if (!sprite.texture) {
        LOG_ERROR("Failed to create character sprite - texture loading failed");
        printf("Failed to create character sprite\n");
        return sprite;
    }
    
    // Allocate animations
    LOG_DEBUG("Allocating memory for %d animations", ANIM_COUNT);
    sprite.animationCount = ANIM_COUNT;
    sprite.animations = malloc(sizeof(Animation) * ANIM_COUNT);
    if (!sprite.animations) {
        LOG_ERROR("Failed to allocate memory for animations");
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
    LOG_DEBUG("Setting up initial sprite state");
    sprite.currentAnimation = ANIM_IDLE;
    sprite.currentFrame = 0;
    sprite.animationTimer = 0.0f;
    sprite.isPlaying = true;
    sprite.facingLeft = false;
    
    LOG_DEBUG("Character sprite created successfully");
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
    LOG_DEBUG("renderSprite called at (%d, %d)", x, y);
    
    if (!sprite->texture || sprite->animationCount == 0) {
        LOG_WARNING("renderSprite: Invalid sprite data (texture=%p, animCount=%d)", 
                   (void*)sprite->texture, sprite->animationCount);
        return;
    }
    
    Animation *anim = &sprite->animations[sprite->currentAnimation];
    if (sprite->currentFrame >= anim->frameCount) {
        LOG_WARNING("renderSprite: Invalid frame index %d >= %d", 
                   sprite->currentFrame, anim->frameCount);
        return;
    }
    
    SpriteFrame *frame = &anim->frames[sprite->currentFrame];
    LOG_DEBUG("renderSprite: Using frame %d, src rect (%d,%d,%d,%d)", 
             sprite->currentFrame, frame->x, frame->y, frame->width, frame->height);
    
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
    
    LOG_DEBUG("renderSprite: Dest rect (%d,%d,%d,%d)", 
             dstRect.x, dstRect.y, dstRect.w, dstRect.h);
    
    // Use SDL_RenderCopyEx for flipping support
    SDL_RendererFlip flip = sprite->facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    LOG_DEBUG("renderSprite: Calling SDL_RenderCopyEx...");
    if (SDL_RenderCopyEx(renderer, sprite->texture, &srcRect, &dstRect, 0.0, NULL, flip) != 0) {
        LOG_ERROR("SDL_RenderCopyEx failed: %s", SDL_GetError());
    } else {
        LOG_DEBUG("renderSprite: SDL_RenderCopyEx completed successfully");
    }
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
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    // Initialize logging
    log_init("platformer.log");
    LOG_INFO("Starting platformer game");
    LOG_INFO("Command line args: %d", argc);
    
    // Initialize SDL
    LOG_INFO("Initializing SDL...");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERROR("SDL initialization failed: %s", SDL_GetError());
        printf("SDL initialization failed: %s\n", SDL_GetError());
        log_close();
        return 1;
    }
    LOG_INFO("SDL initialized successfully");
    
    // Initialize SDL_image
    LOG_INFO("Initializing SDL_image...");
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        LOG_ERROR("SDL_image initialization failed: %s", IMG_GetError());
        printf("SDL_image initialization failed: %s\n", IMG_GetError());
        SDL_Quit();
        log_close();
        return 1;
    }
    LOG_INFO("SDL_image initialized successfully");

    // Create window
    LOG_INFO("Creating SDL window (%dx%d)...", WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_Window *window = SDL_CreateWindow(
        "Chipmunk2D Box Collision Demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        LOG_ERROR("Window creation failed: %s", SDL_GetError());
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        log_close();
        return 1;
    }
    LOG_INFO("SDL window created successfully");

    // Create renderer
    LOG_INFO("Creating SDL renderer...");
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        LOG_ERROR("Renderer creation failed: %s", SDL_GetError());
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        log_close();
        return 1;
    }
    LOG_INFO("SDL renderer created successfully");

    // Create Chipmunk space
    LOG_INFO("Creating Chipmunk physics space...");
    cpSpace *space = cpSpaceNew();
    if (!space) {
        LOG_ERROR("Failed to create Chipmunk space");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        log_close();
        return 1;
    }
    cpSpaceSetGravity(space, cpv(0, -980)); // Gravity pointing down
    LOG_INFO("Chipmunk physics space created successfully");

    // Create static ground body
    LOG_INFO("Creating ground physics body...");
    cpBody *groundBody = cpSpaceGetStaticBody(space);
    cpShape *ground = cpSegmentShapeNew(groundBody, 
        cpv(0, GROUND_HEIGHT), 
        cpv(WINDOW_WIDTH, GROUND_HEIGHT), 
        0.0f);
    cpShapeSetFriction(ground, 0.3f);
    cpSpaceAddShape(space, ground);
    LOG_INFO("Ground physics body created successfully");

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
    LOG_INFO("Creating player physics body...");
    boxes[boxCount] = createBox(space, cpv(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 50));
    cpBody *playerBody = boxes[0].body; // Keep reference to player body
    cpShape *playerShape = boxes[0].shape; // Keep reference to player shape
    boxCount++;
    LOG_INFO("Player physics body created successfully");
    
    // Create player sprite
    LOG_INFO("Loading player sprite...");
    Sprite playerSprite = createCharacterSprite(renderer);
    LOG_INFO("Player sprite loaded successfully");

    // Check initial window state
    Uint32 windowFlags = SDL_GetWindowFlags(window);
    LOG_INFO("Window flags: 0x%08X", windowFlags);
    if (windowFlags & SDL_WINDOW_INPUT_FOCUS) {
        LOG_INFO("Window has input focus");
    } else {
        LOG_WARNING("Window does NOT have input focus");
    }
    
    // Try to ensure window focus on Windows
    SDL_RaiseWindow(window);
    SDL_SetWindowInputFocus(window);
    
    // Enable text input for debugging
    SDL_StartTextInput();
    LOG_INFO("Text input enabled");
    
    // Pump events to ensure everything is initialized
    SDL_PumpEvents();
    LOG_INFO("Initial event pump completed");
    
    // Main loop
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();
    int frameCount = 0;
    LOG_INFO("Entering main game loop");
    
    while (running) {
        frameCount++;
        
        // Log first few frames for debugging
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d starting...", frameCount);
        }
        
        Uint32 currentTime = SDL_GetTicks();
        cpFloat dt = (currentTime - lastTime) / 1000.0f;
        
        // Handle first frame case where dt would be 0
        if (dt <= 0.0f) {
            dt = 0.016f; // Default to 60 FPS
            LOG_DEBUG("First frame or invalid dt, using default: %f", dt);
        }
        
        lastTime = currentTime;
        
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: dt=%f, starting event handling...", frameCount, dt);
        }

        // Handle events
        int eventCount = 0;
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Starting SDL_PollEvent loop...", frameCount);
        }
        
        // Check keyboard state directly (Windows debugging) - more frequent for troubleshooting
        if (frameCount % 30 == 0) { // Every 0.5 seconds for better responsiveness
            const Uint8* keystate = SDL_GetKeyboardState(NULL);
            LOG_INFO("Direct keyboard state check - A:%d D:%d W:%d SPACE:%d LEFT:%d RIGHT:%d UP:%d", 
                     keystate[SDL_SCANCODE_A], keystate[SDL_SCANCODE_D], keystate[SDL_SCANCODE_W], 
                     keystate[SDL_SCANCODE_SPACE], keystate[SDL_SCANCODE_LEFT], keystate[SDL_SCANCODE_RIGHT], 
                     keystate[SDL_SCANCODE_UP]);
            
            // If any keys are pressed, use direct input (bypass SDL events completely)
            if (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT]) {
                leftPressed = true;
                LOG_INFO("Direct input: LEFT key detected via keyboard state polling");
            } else if (leftPressed) {
                leftPressed = false;
                LOG_INFO("Direct input: LEFT key released via keyboard state polling");
            }
            
            if (keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) {
                rightPressed = true;
                LOG_INFO("Direct input: RIGHT key detected via keyboard state polling");
            } else if (rightPressed) {
                rightPressed = false;
                LOG_INFO("Direct input: RIGHT key released via keyboard state polling");
            }
            
            if (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_SPACE]) {
                jumpPressed = true;
                LOG_INFO("Direct input: JUMP key detected via keyboard state polling");
            } else if (jumpPressed) {
                jumpPressed = false;
                LOG_INFO("Direct input: JUMP key released via keyboard state polling");
            }
        }
        
        // Log keyboard state every 60 frames (once per second)
        if (frameCount % 60 == 0) {
            LOG_INFO("Frame %d: Current input state - left:%d right:%d jump:%d", 
                     frameCount, leftPressed, rightPressed, jumpPressed);
        }
        
        while (SDL_PollEvent(&event)) {
            eventCount++;
            
            // Log ALL events for debugging (not just first 3 frames)
            const char* eventTypeName = "UNKNOWN";
            switch (event.type) {
                case SDL_QUIT: eventTypeName = "QUIT"; break;
                case SDL_KEYDOWN: eventTypeName = "KEYDOWN"; break;
                case SDL_KEYUP: eventTypeName = "KEYUP"; break;
                case SDL_MOUSEBUTTONDOWN: eventTypeName = "MOUSEBUTTONDOWN"; break;
                case SDL_MOUSEBUTTONUP: eventTypeName = "MOUSEBUTTONUP"; break;
                case SDL_MOUSEMOTION: eventTypeName = "MOUSEMOTION"; break;
                case SDL_WINDOWEVENT: eventTypeName = "WINDOWEVENT"; break;
                case SDL_TEXTINPUT: eventTypeName = "TEXTINPUT"; break;
                default: break;
            }
            
            // Log every event for first 10 seconds, then only important ones
            bool shouldLog = (frameCount <= 600) || 
                           (event.type == SDL_KEYDOWN) || 
                           (event.type == SDL_KEYUP) || 
                           (event.type == SDL_QUIT) ||
                           (event.type == SDL_MOUSEBUTTONDOWN) ||
                           (event.type == SDL_MOUSEMOTION); // Always log mouse motion for debugging
            
            if (shouldLog) {
                LOG_DEBUG("Event %d: Type=%s (%d)", eventCount, eventTypeName, event.type);
                
                // Add detailed logging for mouse motion events to detect misclassified keyboard events
                if (event.type == SDL_MOUSEMOTION) {
                    LOG_DEBUG("MOUSEMOTION: x=%d y=%d xrel=%d yrel=%d state=0x%08X", 
                             event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel, event.motion.state);
                    
                    // Check for suspicious mouse motion that might be misclassified keyboard input
                    // Keyboard events misclassified as mouse motion often have unusual patterns
                    if (event.motion.xrel == 0 && event.motion.yrel == 0 && event.motion.state == 0) {
                        LOG_WARNING("Suspicious MOUSEMOTION event (no movement, no buttons) - possible misclassified keyboard input");
                    }
                    
                    // SDL scancodes are typically 0-255, so anything >255 is likely not a scancode
                    // Let's look for patterns that might indicate misclassified keyboard input
                    if (event.motion.x >= 0 && event.motion.x <= 255 && event.motion.y == 0) {
                        int scancode = event.motion.x;
                        LOG_WARNING("Possible keyboard scancode %d detected in MOUSEMOTION x coordinate", scancode);
                        
                        // Try to convert suspicious mouse motion to keyboard input
                        // SDL scancodes: A=4, D=7, W=26, SPACE=44, LEFT=80, RIGHT=79, UP=82
                        bool isKeyPress = true; // Assume key press for now
                        
                        switch (scancode) {
                            case 4:   // A key (SDL_SCANCODE_A)
                            case 80:  // LEFT arrow (SDL_SCANCODE_LEFT)
                                leftPressed = isKeyPress;
                                LOG_INFO("Recovered LEFT key %s from misclassified MOUSEMOTION", 
                                        isKeyPress ? "press" : "release");
                                break;
                            case 7:   // D key (SDL_SCANCODE_D)
                            case 79:  // RIGHT arrow (SDL_SCANCODE_RIGHT)
                                rightPressed = isKeyPress;
                                LOG_INFO("Recovered RIGHT key %s from misclassified MOUSEMOTION", 
                                        isKeyPress ? "press" : "release");
                                break;
                            case 26:  // W key (SDL_SCANCODE_W)
                            case 82:  // UP arrow (SDL_SCANCODE_UP)
                            case 44:  // SPACE (SDL_SCANCODE_SPACE)
                                jumpPressed = isKeyPress;
                                LOG_INFO("Recovered JUMP key %s from misclassified MOUSEMOTION", 
                                        isKeyPress ? "press" : "release");
                                break;
                            default:
                                if (scancode <= 255) {
                                    LOG_DEBUG("Valid SDL scancode %d in MOUSEMOTION but not mapped to game input", scancode);
                                } else {
                                    LOG_DEBUG("Invalid scancode %d in MOUSEMOTION - outside valid range", scancode);
                                }
                                break;
                        }
                    } else if (event.motion.x > 255) {
                        LOG_DEBUG("Large x coordinate %d in MOUSEMOTION - likely actual mouse movement", event.motion.x);
                    }
                    
                    // Alternative pattern: check if scancodes appear in different fields
                    if (event.motion.y > 0 && event.motion.y < 512 && event.motion.x == 0) {
                        LOG_WARNING("Possible keyboard scancode %d detected in MOUSEMOTION y coordinate", event.motion.y);
                    }
                    
                    // Check for unusual relative motion that might indicate misclassified keys
                    if (abs(event.motion.xrel) > 100 || abs(event.motion.yrel) > 100) {
                        LOG_WARNING("Large relative motion detected: xrel=%d yrel=%d - possible misclassified input", 
                                   event.motion.xrel, event.motion.yrel);
                    }
                }
            }
            
            if (eventCount > 100) {
                LOG_WARNING("Too many events in one frame: %d", eventCount);
                break; // Prevent infinite loop
            }
            
            if (event.type == SDL_QUIT) {
                LOG_INFO("Quit event received");
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                LOG_INFO("Mouse button %d down at (%d, %d)", event.button.button, event.button.x, event.button.y);
                if (event.button.button == SDL_BUTTON_LEFT && boxCount < MAX_BOXES) {
                    // Add bounds checking for mouse position
                    if (event.button.x >= 0 && event.button.x < WINDOW_WIDTH && 
                        event.button.y >= 0 && event.button.y < WINDOW_HEIGHT) {
                        cpVect mousePos = sdlToCP(event.button.x, event.button.y);
                        LOG_DEBUG("Creating box at mouse position: SDL(%d,%d) -> Chipmunk(%.2f,%.2f)", 
                                 event.button.x, event.button.y, mousePos.x, mousePos.y);
                        boxes[boxCount] = createBox(space, mousePos);
                        boxCount++;
                        LOG_DEBUG("Box created successfully. Total boxes: %d", boxCount);
                    } else {
                        LOG_WARNING("Mouse click outside window bounds: (%d,%d)", event.button.x, event.button.y);
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                LOG_INFO("KEYDOWN: sym=%d scancode=%d mod=%d repeat=%d", 
                        event.key.keysym.sym, event.key.keysym.scancode, 
                        event.key.keysym.mod, event.key.repeat);
                        
                // Check if key is actually one we care about
                bool isMovementKey = (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_LEFT ||
                                    event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT ||
                                    event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP ||
                                    event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_F1);
                                    
                if (!isMovementKey) {
                    LOG_WARNING("Unhandled key down: %d", event.key.keysym.sym);
                }
                
                switch (event.key.keysym.sym) {
                    case SDLK_F1:
                        showDebug = !showDebug;
                        LOG_INFO("F1 pressed - Debug visualization: %s", showDebug ? "ON" : "OFF");
                        printf("Debug visualization: %s\n", showDebug ? "ON" : "OFF");
                        break;
                    case SDLK_a:
                    case SDLK_LEFT:
                        leftPressed = true;
                        LOG_INFO("LEFT movement key pressed (was: %s)", leftPressed ? "already pressed" : "false");
                        break;
                    case SDLK_d:
                    case SDLK_RIGHT:
                        rightPressed = true;
                        LOG_INFO("RIGHT movement key pressed (was: %s)", rightPressed ? "already pressed" : "false");
                        break;
                    case SDLK_w:
                    case SDLK_UP:
                    case SDLK_SPACE:
                        jumpPressed = true;
                        LOG_INFO("JUMP key pressed (was: %s)", jumpPressed ? "already pressed" : "false");
                        break;
                    default:
                        // Already logged as unhandled above
                        break;
                }
            } else if (event.type == SDL_KEYUP) {
                LOG_INFO("KEYUP: sym=%d scancode=%d mod=%d", 
                        event.key.keysym.sym, event.key.keysym.scancode, event.key.keysym.mod);
                        
                switch (event.key.keysym.sym) {
                    case SDLK_a:
                    case SDLK_LEFT:
                        leftPressed = false;
                        LOG_INFO("LEFT movement key released");
                        break;
                    case SDLK_d:
                    case SDLK_RIGHT:
                        rightPressed = false;
                        LOG_INFO("RIGHT movement key released");
                        break;
                    case SDLK_w:
                    case SDLK_UP:
                    case SDLK_SPACE:
                        jumpPressed = false;
                        LOG_INFO("JUMP key released");
                        break;
                    default:
                        LOG_DEBUG("Unhandled key up: %d", event.key.keysym.sym);
                        break;
                }
            } else if (event.type == SDL_WINDOWEVENT) {
                LOG_DEBUG("WINDOWEVENT: event=%d", event.window.event);
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    LOG_WARNING("Window lost focus!");
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    LOG_INFO("Window gained focus");
                }
            } else if (shouldLog) {
                LOG_DEBUG("Other event type: %d", event.type);
            }
        }
        
        // Log event summary
        if (frameCount <= 3 || eventCount > 0) {
            LOG_DEBUG("Frame %d: Event handling complete, processed %d events", frameCount, eventCount);
        }
        
        // Check if no events are being received
        if (frameCount % 300 == 0 && eventCount == 0) {
            LOG_WARNING("Frame %d: No events received for 5 seconds - possible input issue", frameCount);
        }

        // Update player movement
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Starting player movement update...", frameCount);
        }
        if (frameCount % 60 == 0) {
            LOG_DEBUG("Frame %d: Input state - left:%d right:%d jump:%d", 
                     frameCount, leftPressed, rightPressed, jumpPressed);
        }
        
        // Removed old fallback keyboard polling - now using more frequent direct polling above
        
        updatePlayerMovement(space, playerBody, playerShape, leftPressed, rightPressed, jumpPressed);
        
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Starting sprite animation update...", frameCount);
        }
        
        // Update player sprite animation based on movement state
        cpVect vel = cpBodyGetVelocity(playerBody);
        bool onGround = isOnGround(space, playerBody, playerShape);
        
        // Update sprite direction based on velocity
        if (vel.x < -5.0f) {
            playerSprite.facingLeft = true;
        } else if (vel.x > 5.0f) {
            playerSprite.facingLeft = false;
        }
        // Don't change direction if velocity is too small (preserve last direction)
        
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
        
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Starting physics update...", frameCount);
        }
        
        // Update physics
        // Additional safety check - this shouldn't happen now but keep as backup
        if (dt > 0.033f) {
            LOG_WARNING("Large dt detected: %f, clamping to 0.033", dt);
            dt = 0.033f;
        }
        
        // Log physics step occasionally for debugging
        if (frameCount % 300 == 0) {
            LOG_DEBUG("Physics step with dt: %f", dt);
        }
        
        cpSpaceStep(space, dt);
        
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Starting render...", frameCount);
        }

        // Clear screen
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Setting clear color and clearing screen...", frameCount);
        }
        if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) != 0) {
            LOG_ERROR("SDL_SetRenderDrawColor failed: %s", SDL_GetError());
        }
        if (SDL_RenderClear(renderer) != 0) {
            LOG_ERROR("SDL_RenderClear failed: %s", SDL_GetError());
        }
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Screen cleared successfully", frameCount);
        }

        // Draw ground
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Drawing ground...", frameCount);
        }
        if (SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255) != 0) {
            LOG_ERROR("SDL_SetRenderDrawColor for ground failed: %s", SDL_GetError());
        }
        SDL_Rect groundRect = {
            0, 
            WINDOW_HEIGHT - GROUND_HEIGHT, 
            WINDOW_WIDTH, 
            GROUND_HEIGHT
        };
        if (SDL_RenderFillRect(renderer, &groundRect) != 0) {
            LOG_ERROR("SDL_RenderFillRect for ground failed: %s", SDL_GetError());
        }
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Ground drawn successfully", frameCount);
        }

        // Draw all boxes
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Drawing %d boxes/sprites...", frameCount, boxCount);
        }
        for (int i = 0; i < boxCount; i++) {
            cpVect pos = cpBodyGetPosition(boxes[i].body);
            cpFloat angle = cpBodyGetAngle(boxes[i].body);
            
            int x, y;
            cpToSDL(pos, &x, &y);
            
            if (frameCount <= 3) {
                LOG_DEBUG("Frame %d: Drawing box %d at (%d, %d)", frameCount, i, x, y);
            }
            
            if (i == 0) {
                // Draw player as animated sprite
                if (frameCount <= 3) {
                    LOG_DEBUG("Frame %d: Rendering player sprite...", frameCount);
                }
                renderSprite(renderer, &playerSprite, x, y);
                if (frameCount <= 3) {
                    LOG_DEBUG("Frame %d: Player sprite rendered successfully", frameCount);
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
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Presenting rendered frame...", frameCount);
        }
        SDL_RenderPresent(renderer);
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Frame presented successfully", frameCount);
        }
        
        // Log every second
        if (frameCount % 60 == 0) {
            LOG_INFO("Frame %d: Running at approximately 60 FPS", frameCount);
        }
        
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Frame completed successfully", frameCount);
        }

        // Cap framerate
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Starting frame delay...", frameCount);
        }
        SDL_Delay(16); // ~60 FPS
        if (frameCount <= 3) {
            LOG_DEBUG("Frame %d: Frame delay completed", frameCount);
        }
    }

    // Cleanup
    LOG_INFO("Starting cleanup...");
    destroySprite(&playerSprite);
    for (int i = 0; i < boxCount; i++) {
        cpShapeFree(boxes[i].shape);
        cpBodyFree(boxes[i].body);
    }
    cpShapeFree(ground);
    cpSpaceFree(space);
    
    LOG_INFO("Destroying SDL resources...");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    
    LOG_INFO("Exiting cleanly");
    log_close();

    return 0;
}