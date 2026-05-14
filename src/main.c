#include <stdlib.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>

#define SEC_IN_NANO 1000000000
#define FPS 60 
#define DELTA_TIME SEC_IN_NANO / FPS
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

#define do_180(dir) ~(dir^-2) // change direction without ifelse lol super risk

#define BALL_QUANTITY 1

#define panic(format, ...) fprintf(stderr, "[ERROR] " format "\n", ##__VA_ARGS__); exit(EXIT_FAILURE)
#define debug(format, ...) fprintf(stderr, "[DEBUG] " format "\n", ##__VA_ARGS__)

#define length(arr) sizeof(arr)/sizeof(arr[0])

static const uint64_t getTimeInNano() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (SEC_IN_NANO * time.tv_sec) + time.tv_nsec;
}


typedef struct { 
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

#define color(r, g, b, a) (Color){r, g, b, a}

static const Color colors[] = {
    color(0xff, 0xff, 0xff, 0xff),
    color(0xff, 0, 0, 0xff),
    color(0, 0xff, 0, 0xff),
    color(0, 0, 0xff, 0xff),
    color(0xff, 0xff, 0, 0xff),
    color(0xdd, 0xaa, 0xcc, 0xff),
    color(0xcc, 0xcc, 0xcc, 0xff),
    color(0x18, 0x18, 0x18, 0xff),
    color(0, 0, 0, 0xff),
    color(0xAA, 0xDD, 0, 0xff),
};

#define BACKGROUND colors[0]
#define RED colors[1]
#define GREEN colors[2]
#define BLUE  colors[3]
#define YELLOW colors[4]


#define sdl_color(color) color.r, color.g, color.b, color.a

typedef struct {
    float x;
    float y;
} Point;

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    FREEZE,
} Direction;

typedef struct Player {
    uint8_t life;
    Direction dir;
    SDL_FRect sprite;
    Color color;
    float velocity;
} Player;

typedef struct Ball {
    SDL_FRect sprite;
    Direction dirs[2];
    Color color;
    int velocity[2];
} Ball;

static struct {
    SDL_Window *window;
    SDL_Renderer *render;
    uint8_t running;
    Player players[2];
    Ball balls[BALL_QUANTITY];
    const uint8_t *key_states;
    int width;
    int height;
} game = {0};

static inline int pick_random(int from, int to) {
    return rand() % (to-from) + from; 
}   
static inline const Direction direction_pick_random() {
    return pick_random(0, FREEZE);
}

static inline const Direction direction_pick_randomX() {
    return pick_random(LEFT, FREEZE);
}

static inline const Direction direction_pick_randomY() {
    return pick_random(UP, LEFT);
}

static inline Color color_pick_random() {
    return colors[rand() % (length(colors) - 1) + 1];
}


static inline void player_move(Player * const p, const SDL_Scancode up, const SDL_Scancode left, const SDL_Scancode right, const SDL_Scancode down) { 
    if (game.key_states[up]) {
        p->sprite.y -= p->velocity;  
    }

    if (game.key_states[left]) { 
        p->sprite.x -= p->velocity;
    }  

    if (game.key_states[right]) {
        p->sprite.x += p->velocity;
    }

    if (game.key_states[down]) {
        p->sprite.y += p->velocity;
    }
    
}

static void players_move_event() {

    player_move(&game.players[0], SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_S);
    player_move(&game.players[1], SDL_SCANCODE_I, SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_K);
    
    if (game.key_states[SDL_SCANCODE_Q]) 
        game.running = false;
}  

static void eventsHandler() {
    // SDL_Event e;
    // SDL_PollEvent(&e);
    // switch (e.type)
    // {
    // case SDL_EVENT_QUIT:
    //     game.running = 0;
    //     break;
    // case SDL_EVENT_KEY_DOWN:
    // case SDL_EVENT_KEY_UP:
    //     fetchState();
    // break;
    // default:
    //     break;
    // }
    SDL_PumpEvents();
}

static inline unsigned char checkBoxCollision(SDL_FRect a, SDL_FRect b) {
    return (a.x < b.x+b.w   && // crossing X
            a.x + a.w > b.x && // ----------
            a.y < b.y+b.h   && // crossing Y
            a.y+a.h > b.y);    // ----------
}

static inline unsigned char directionCollision(SDL_FRect r1, SDL_FRect r2) {
    float top = r1.y;
    float higher = r2.y;
    float downer = r2.y+r2.h;
    
    if (higher < top && top < downer) {
        debug("R2 saying: I'm on top")
    }
    return UP;
} 

static void game_create_balls() {
    for (size_t i = 0; i < BALL_QUANTITY; i++ ) {
        game.balls[i] = (Ball) { 
            .sprite = (SDL_FRect) {.w = 50, .h = 50, .x=game.width/2-50, .y=game.height/2-50 },
            .color = color_pick_random(),
            .dirs = {direction_pick_randomX(), direction_pick_randomY()},
            .velocity = {pick_random(1, 2), pick_random(1, 2)}
        };
    }
}

static void gameSetup() {
    int w, h = 0;
    SDL_GetWindowSize(game.window, &w, &h);
    game.width = w;
    game.height = h;
    game.players[0] = (Player){
        .life = 100,
        .dir = FREEZE,
        .sprite = (SDL_FRect) {.w = 100, .h = 200, .x = w/100, .y = h/100},
        .color = RED,
        .velocity = 5.0f
    };
    game.players[1] = (Player){
        .life = 100,
        .dir = FREEZE,
        .sprite = (SDL_FRect) {.w = 100, .h = 200, .x = w-200 , .y = h-200},
        .color = BLUE,
        .velocity = 5.0f
    };
    game_create_balls();
}

static void players_draw() {
    for (size_t i = 0; i < length(game.players); i++) {
        SDL_SetRenderDrawColor(game.render, sdl_color(game.players[i].color));
        SDL_RenderFillRect(game.render, &game.players[i].sprite);
    
    } 
}

static void ball_draw() {
    for (size_t i = 0; i < length(game.balls); i++) {
        SDL_SetRenderDrawColor(game.render, sdl_color(game.balls[i].color));
        SDL_RenderFillRect(game.render, &game.balls[i].sprite);
    }
}


static void ball_reset() {
    for (int i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i]; 
        ball->sprite.x = (game.width/2)  - ball->sprite.w; // position at middle  
        ball->sprite.y = (game.height/2) - ball->sprite.h;
        ball->dirs[0] = do_180(ball->dirs[0]);
        ball->dirs[1] = ~(ball->dirs[1]^-2);
        ball->velocity[0] = 5;
        ball->velocity[1] = 5;
    }
}

static void ball_move() {

    for (int i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i];
        
        if (ball->sprite.y <= 0) {
            ball->dirs[1] = DOWN;
        } else if (ball->sprite.y+ball->sprite.h >= game.height) {
            ball->dirs[1] = UP;
        }
        
        if (ball->sprite.x <= 0) {
            // ball_reset();
            ball->dirs[0] = RIGHT;

        } else if (ball->sprite.x+ball->sprite.w >= game.width) {
            // ball_reset();
            ball->dirs[0] = LEFT;
        }
        
        for (int i = 0; i < length(ball->dirs); i++) {
            switch (ball->dirs[i])
            {
                case UP:
                ball->sprite.y -= ball->velocity[1];
                break;
                case LEFT:
                ball->sprite.x -= ball->velocity[0];
                break;
                case RIGHT:
                ball->sprite.x += ball->velocity[0];
                break;
                case DOWN:
                ball->sprite.y += ball->velocity[1];
                break;
                default:
                break;
            }
        }
    }
}

static void player_check_ball_colission() {
    for (size_t i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i]; 
        
        for (size_t i = 0; i < length(game.players); i++) {
            Player *player = &game.players[i];
            if (checkBoxCollision(player->sprite, ball->sprite)) {
                if (ball->sprite.y > player->sprite.y+(player->sprite.h/2)) {
                    ball->dirs[1] = DOWN; // ball up collision
                    // player on top ball on bottom
                    // ball->sprite.y = player->sprite.y + player->sprite.h;
                }
                else {
                    ball->dirs[1] = UP;
                    // ball->sprite.y = player->sprite.y - ball->sprite.h;
                };
                if (ball->sprite.x+(ball->sprite.w*0.5) > player->sprite.x+(player->sprite.w*0.5)) { // ball left collision
                    ball->dirs[0] = LEFT;
                    // player on left ball on right
                    // ball->sprite.x = player->sprite.x+player->sprite.w;
                }
                else {
                    ball->dirs[0] = LEFT;
                    // ball->sprite.x = player->sprite.x - ball->sprite.w;
                }

                if (ball->sprite.y + ball->sprite.h > player->sprite.y ) {
                    debug("ball-down: %f, player-up: %f ", ball->sprite.y+ball->sprite.h, player->sprite.y);
                } 
                if (ball->sprite.y > player->sprite.y + player->sprite.h) {
                    debug("ball-up: %f, player-down: %f ", ball->sprite.y, player->sprite.y+player->sprite.h);
                } 

            }
        }
    }

} 

int main(int argc, char *argv[]) {
    
    srand(time(NULL));
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        panic("%s", SDL_GetError());
    }
    
    if (!SDL_CreateWindowAndRenderer("slopp ass game", DEFAULT_WIDTH, DEFAULT_HEIGHT, 0, &game.window, &game.render)) {
        panic("%s", SDL_GetError());
    }
    
    gameSetup();
    game.running = 1;
    game.key_states = (const unsigned char *)SDL_GetKeyboardState(NULL);
    
    while (game.running) {
        uint64_t start = getTimeInNano();
        SDL_SetRenderDrawColor(game.render, sdl_color(BACKGROUND));
        SDL_RenderClear(game.render);
        
        eventsHandler();

        
        players_move_event();
        player_check_ball_colission();
        ball_move();


        ball_draw();
        players_draw();

        SDL_RenderPresent(game.render);

        uint64_t end = getTimeInNano();
        uint64_t delta = end - start;
        if (delta < DELTA_TIME) {
            usleep((DELTA_TIME - delta) / 1000);
        }
    }

    SDL_DestroyRenderer(game.render);
    SDL_DestroyWindow(game.window);
    SDL_Quit();
    return 0;
}