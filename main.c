#define _USE_MATH_DEFINES
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <chipmunk/chipmunk.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

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
    SDL_Texture *texture = IMG_LoadTexture(renderer, "./assets/characters.png");
    if (!texture) {
        printf("Failed to load character spritesheet: %s\n", IMG_GetError());
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
        printf("Failed to create character sprite\n");
        return sprite;
    }
    
    // Allocate animations
    sprite.animationCount = ANIM_COUNT;
    sprite.animations = malloc(sizeof(Animation) * ANIM_COUNT);
    
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
    if (!sprite->texture || sprite->animationCount == 0) return;
    
    Animation *anim = &sprite->animations[sprite->currentAnimation];
    if (sprite->currentFrame >= anim->frameCount) return;
    
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
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image initialization failed: %s\n", IMG_GetError());
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
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create Chipmunk space
    cpSpace *space = cpSpaceNew();
    cpSpaceSetGravity(space, cpv(0, -980)); // Gravity pointing down

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
    cpBody *playerBody = boxes[0].body; // Keep reference to player body
    cpShape *playerShape = boxes[0].shape; // Keep reference to player shape
    boxCount++;
    
    // Create player sprite
    Sprite playerSprite = createCharacterSprite(renderer);

    // Main loop
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();
    
    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        cpFloat dt = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT && boxCount < MAX_BOXES) {
                    cpVect mousePos = sdlToCP(event.button.x, event.button.y);
                    boxes[boxCount] = createBox(space, mousePos);
                    boxCount++;
                }
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_F1:
                        showDebug = !showDebug;
                        printf("Debug visualization: %s\n", showDebug ? "ON" : "OFF");
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
        
        // Update physics
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
                renderSprite(renderer, &playerSprite, x, y);
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

    return 0;
}