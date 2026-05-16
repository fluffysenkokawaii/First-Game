#ifndef GAME_DEFINITIONS_H_
#define GAME_DEFINITIONS_H_

#define panic(format, ...) fprintf(stderr, "[ERROR] " format "\n", ##__VA_ARGS__); exit(EXIT_FAILURE)
#define debug(format, ...) fprintf(stderr, "[DEBUG] " format "\n", ##__VA_ARGS__)
#define length(arr) sizeof(arr)/sizeof(arr[0])

#define game_tb(n) game.texture_buffer[n]

#define SEC_IN_NANO 1000000000
#define MILLI_IN_MICRO 1000
#define DELTA_TIME SEC_IN_NANO / FPS

typedef struct { 
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

#define color(r, g, b, a) (Color){r, g, b, a}





#define sdl_color(color) color.r, color.g, color.b, color.a
#define ttf_color(color) (SDL_Color) {.r = color.r, .g = color.g, .b = color.b, .a = color.a}

#define do_180(dir) ~(dir^-2) // change direction without ifelse lol super risk, you may not be doing this :D  
typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    FREEZE,
} Direction2d;


typedef enum {
    COLLISSION_NONE = 0,
    COLLISION_TOP = 1,
    COLLISION_BOTTOM = 2,
    COLLISION_RIGHT = 4,
    COLLISION_LEFT = 8,
} DirectionCollision; // some bs here ::


typedef struct {
    Direction2d x;
    Direction2d y;
} Directions2d;

typedef struct Player {
    uint8_t life;
    SDL_FRect sprite;
    Color color;
    float velocity;
    uint32_t points;
} Player;

typedef struct Ball {
    SDL_FRect sprite;
    Directions2d dirs;
    Color color;
    int velocity[2];
} Ball;

typedef struct {
    uint8_t *data;
    uint32_t size;
} Sound;


typedef enum {
    SCREEN_INTRO,
    SCREEN_GAME,
} Screen;


typedef struct {
    uint64_t general;
} Timers;

#endif