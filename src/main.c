#include <stdlib.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "config.h"

static struct {
    SDL_Window *window;
    SDL_Renderer *render;
    uint8_t running;
    Player players[2];
    Ball balls[BALL_QUANTITY];
    const uint8_t *key_states;
    int width;
    int height;
    TTF_Font* font;
    SDL_Texture *score;
    Sound sound;
    SDL_AudioStream* audio_stream; 
    SDL_AudioSpec audio_spec;
    SDL_Texture * texture_buffer[10];
    Screen current_screen;
    Timers timer;
} game = {0};

static inline Color color_pick_random() {
    return colors[rand() % (length(colors) - 1) + 1];
}

static inline int pick_random(int from, int to) {
    return rand() % (to-from) + from; 
}   

// static inline const Direction2d direction_pick_random() {
//     return pick_random(0, FREEZE);
// }

static inline const Direction2d direction_pick_randomX() {
    return pick_random(LEFT, FREEZE);
}

static inline const Direction2d direction_pick_randomY() {
    return pick_random(UP, LEFT);
}




static const uint64_t getTimeInNano() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (SEC_IN_NANO * time.tv_sec) + time.tv_nsec;
}

static inline void player_move(Player * const p, Direction2d dir) { 
    switch (dir)
    {
    case UP:
        p->sprite.y -= p->velocity;  
        break;
    case LEFT:
        p->sprite.x -= p->velocity;
        break;
    case RIGHT:
        p->sprite.x += p->velocity;
        break;
    case DOWN:
        p->sprite.y += p->velocity;
        break;
    default:
        break;
    }
}

static void game_balls_create() {
    for (size_t i = 0; i < BALL_QUANTITY; i++ ) {
        game.balls[i] = (Ball) { 
            .sprite = (SDL_FRect) {.w = BALL_WIDTH, .h = BALL_HEIGHT, .x=game.width/2-BALL_WIDTH, .y=game.height/2-BALL_HEIGHT},
            .color = BALLS_COLOR,
            .dirs = {direction_pick_randomX(), direction_pick_randomY()},
            .velocity = {pick_random(1, 5), pick_random(1, 5)}
        };
    }
}

void gameplay_reset() {
    const int w = game.width;
    const int h = game.height; 
    const float pw = PLAYERS_W;
    const float ph = PLAYERS_H;

    game.players[0] = (Player){
        #ifndef VERTICAL_POINTS
        .sprite = (SDL_FRect) {.w = pw, .h = ph, .x = w/6 - pw/2, .y = h/2 - ph/2},
        #else
        .sprite = (SDL_FRect) {.w = pw, .h = ph, .x = w/2 - pw/2, .y = h/6 - ph/2},
        #endif
        .color = PLAYER_1_COLOR,
        .velocity = PLAYERS_VELOCITY,
        .points = 0,
    };
    game.players[1] = (Player){
        #ifndef VERTICAL_POINTS
        .sprite = (SDL_FRect) {.w = pw, .h = ph, .x = w - pw*2 , .y = h/2 - ph/2},
        #else
        .sprite = (SDL_FRect) {.w = pw, .h = ph, .x = w/2 - pw/2 , .y = h - ph*1.5},
        #endif
        .color = PLAYER_2_COLOR,
        .velocity = PLAYERS_VELOCITY,
        .points = 0,
    };
    game_balls_create();
}


static void eventsHandler() {
    SDL_PumpEvents();
}

// static inline unsigned char collision_box(SDL_FRect a, SDL_FRect b) {
//     return (a.x < b.x+b.w   && // crossing X
//             a.x + a.w > b.x && // ----------
//             a.y < b.y+b.h   && // crossing Y
//             a.y+a.h > b.y);    // ----------
// }

static inline unsigned char collision_direction(SDL_FRect r1, SDL_FRect r2, Directions2d *dirs) {
    float top = r1.y;
    float bottom = r1.y+r1.h;
    float higher = r2.y;
    float downer = r2.y+r2.h;
    float lefter = r1.x;
    float righter = r1.x+r1.w;
    float xdot = r2.x;
    // float ydot = r2.y;
    float xdotfinnish = r2.x+r2.w;
    
    unsigned char ret = 0;

    if ((xdot < lefter && lefter < xdotfinnish) || (lefter < xdot && xdot < righter)) {
        if (higher < top && top < downer) {
            dirs->y = UP;
            ret += 1;
        } else if (higher < bottom && bottom < downer) {
            dirs->y = DOWN;
            ret += 2;
        }
    }

    if ((top < higher && higher < bottom) || (higher < top && top < downer)) {
        if (xdot < righter && righter < xdotfinnish) {
            dirs->x = RIGHT;
            ret += 4; 
        } else if (xdot < lefter && lefter < xdotfinnish) {
            dirs->x = LEFT;
            ret += 8;
        } 
    }
    return ret;
} 



static void game_sound_score() {
    if (!SDL_PutAudioStreamData(game.audio_stream, game.sound.data, game.sound.size)) {
        panic("%s", SDL_GetError());
    }
}

static void game_refresh_score() {
    if (game.score != NULL) {
        SDL_DestroyTexture(game.score);
    }
    char score[64];
    snprintf(score, 64, "Score: | %d : %d |", game.players[0].points, game.players[1].points);
    SDL_Surface *sir = TTF_RenderText_Solid(game.font, score, 0, ttf_color(WHITE));
    game.score = SDL_CreateTextureFromSurface(game.render, sir);
    SDL_DestroySurface(sir);
}


static void game_register_text(const char *text, Color color, SDL_Texture** buf) {
    if (*buf != NULL) {
        SDL_DestroyTexture(*buf);
    }
    SDL_Surface *sir = TTF_RenderText_Solid(game.font, text, 0, ttf_color(color));
    *buf = SDL_CreateTextureFromSurface(game.render, sir);
    SDL_DestroySurface(sir);
} 

#define game_sleep(ms) usleep(MILLI_IN_MICRO * ms)


static void game_setup() {
    SDL_GetWindowSize(game.window, &game.width, &game.height);

    gameplay_reset();
  
    game.font = TTF_OpenFont("./fonts/DepartureMonoNerdFontPropo-Regular.otf", 32);
    if (game.font == NULL) {
        game.font = TTF_OpenFont("../fonts/DepartureMonoNerdFontPropo-Regular.otf", 32);
        if (game.font == NULL) {
            panic("Font not found\n|%s", SDL_GetError());
        }
    }

    game_refresh_score();

    game.audio_spec = (SDL_AudioSpec){
        .channels = 1,
        .format = SDL_AUDIO_S16,
        .freq = 22050,
    };

    game.audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &game.audio_spec, NULL, NULL);
    
    if (!game.audio_stream) {
        panic("Audio %s", SDL_GetError());
    }

    SDL_ResumeAudioStreamDevice(game.audio_stream);

    if (!SDL_LoadWAV("./sounds/"AUDIO_FILE, &game.audio_spec, &game.sound.data, &game.sound.size)) {
        if (!SDL_LoadWAV("../sounds/"AUDIO_FILE, &game.audio_spec, &game.sound.data, &game.sound.size)) {
            panic("Audio %s", SDL_GetError());
        }
    };
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


static void ball_reset(Ball* ball) {
    ball->sprite.x = (game.width/2)  - ball->sprite.w; // position at middle  
    ball->sprite.y = (game.height/2) - ball->sprite.h;
    ball->velocity[0] = pick_random(1, 5);
    ball->velocity[1] = pick_random(1, 5);
    ball->color = BALLS_COLOR;
}

static void ball_move(Ball* ball){
        switch (ball->dirs.x)
        {
        case LEFT: 
            ball->sprite.x -= ball->velocity[0];
            break;
        case RIGHT:
            ball->sprite.x += ball->velocity[0];
            break;
        default:
            break;
        }
        switch (ball->dirs.y)
        {
        case UP:
            ball->sprite.y -= ball->velocity[1];
            break;
        case DOWN:
            ball->sprite.y += ball->velocity[1];
            break;
        default:
            break;
        }
}

static void balls_move() {

    for (int i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i];
        // Borders collision
        if (ball->sprite.x < 0) {
            // left
            ball->dirs.x = RIGHT;
            #ifndef VERTICAL_POINTS   
            game.players[1].points++;
            goto refresh;
            #endif
        } else if (ball->sprite.x+ball->sprite.w > game.width) {
            // right
            ball->dirs.x = LEFT;
            
            #ifndef VERTICAL_POINTS
            game.players[0].points++;
            goto refresh;
            #endif
        }
        if (ball->sprite.y < 0) {
            // up
            ball->dirs.y = DOWN;

            #ifdef VERTICAL_POINTS
            game.players[1].points++;
            goto refresh;
            #endif
        }
        else if (ball->sprite.y+ball->sprite.h > game.height) {
            // down
            ball->dirs.y = UP;
    
            #ifdef VERTICAL_POINTS
            game.players[0].points++;
            goto refresh;
            #endif
        }

        if(0) {
            refresh:
                ball_reset(ball);
                game_refresh_score();
                game_sound_score();
        }

        if (game.key_states[SDL_SCANCODE_0]) {
            debug("Ball position: x: %f, y: %f", ball->sprite.x, ball->sprite.y);
        }

        ball_move(ball);

        
    }
}

static void player_ball_colission() {
    for (size_t i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i];
        for (size_t j = 0; j < length(game.players); j++) {
            Player * player = &game.players[j];
            DirectionCollision direction;
            if ((direction = collision_direction(player->sprite, ball->sprite, &ball->dirs))) {
                // // player based
                if (direction & COLLISION_LEFT) {
                    ball->sprite.x += player->sprite.x - (ball->sprite.x + ball->sprite.w);
                } else if (direction & COLLISION_RIGHT) {
                    ball->sprite.x += player->sprite.x+player->sprite.w - ball->sprite.x; 
                } else if (direction & COLLISION_BOTTOM) {
                    ball->sprite.y -= ball->sprite.y - (player->sprite.y+player->sprite.h);
                } else if (direction & COLLISION_TOP) {
                    ball->sprite.y += player->sprite.y - (ball->sprite.y+ball->sprite.h);
                }
                
                ball->velocity[0] += player->velocity*0.1f;
                ball->velocity[1] += player->velocity*0.1f;
                if (ball->velocity[0] > BALLS_MAX_SPEED) {
                    ball->velocity[0] = BALLS_MAX_SPEED;
                }
                if (ball->velocity[1] > BALLS_MAX_SPEED) {
                    ball->velocity[1] = BALLS_MAX_SPEED;
                }

            };
        }
    }
}

void game_display_score() {
    SDL_RenderTexture(game.render, game.score, NULL, &(SDL_FRect){10, 10, game.width/2, game.height/10});
}


void game_destroy() {
    if (game.score) {
        SDL_DestroyTexture(game.score);
        game.score = NULL;
    }

    if (game.font) {
        TTF_CloseFont(game.font);
        game.font = NULL;
    }

    if (game.render) {
        SDL_DestroyRenderer(game.render);
        game.render = NULL;
    }

    if (game.window) {
        SDL_DestroyWindow(game.window);
        game.window = NULL;
    }

    for (size_t i = 0; i < length(game.texture_buffer); i++) {
        if (game.texture_buffer[i]) {
            SDL_DestroyTexture(game.texture_buffer[i]);
            game.texture_buffer[i] = NULL;
        }
    }

}

static void game_display_text(size_t index, float x, float y) {
    SDL_FRect dst = {0};
    SDL_GetTextureSize(game_tb(index), &dst.w, &dst.h);
    dst.x = x - dst.w;
    dst.y = y - dst.h;
    if (!SDL_RenderTexture(game.render, game_tb(index), NULL, &dst)) {
        panic("%s",SDL_GetError());
    }
}

static void game_startscreen_setup() {
    game_register_text("starting game....", WHITE, &game_tb(0));
    game_register_text("3", WHITE, &game_tb(1));
    game_register_text("2", WHITE, &game_tb(2));
    game_register_text("1", WHITE, &game_tb(3));
    game_register_text("GO", WHITE, &game_tb(4));
    game_register_text("Controls", WHITE, &game_tb(5));
    game_register_text("player 1: W,A,S,D", WHITE, &game_tb(6));
    game_register_text("player 2: I,J,K,L", WHITE, &game_tb(7));
    game_register_text("R = restart", WHITE, &game_tb(8));
}

static void game_startscreen() {

    if (game.key_states[SDL_SCANCODE_Q]) {
        game.running = false;
    }

    uint64_t time = game.timer.general/FPS;
    if (time > 9) {
        game.current_screen = SCREEN_GAME;
        game.timer.general = 0;
    };

    int mx = game.width/2;
    int my = game.height/2;

    game_display_text(5, mx, my+80);
    game_display_text(6, mx*1.5, my+64+64);
    game_display_text(7, mx*1.5, my+64+64+32);
    game_display_text(8, mx*1.2, my+64+64+32+64);

    if (time <=  5) {
        game_display_text(0, mx*1.5, my);
    } else if (time <= 6) {
        game_display_text(1, mx, my);
    } else if (time <= 7) {
        game_display_text(2, mx, my);
    } else if (time <= 8) {
        game_display_text(3, mx, my);
    } else if (time <= 9) {
        game_display_text(4, mx, my);
    } 

    game.timer.general++;
}

static void gameplay_events() {
    #define key_pressed(key) if (game.key_states[key]) 

    Player *p1 = &game.players[0];
    Player *p2 = &game.players[1];

    key_pressed(SDL_SCANCODE_W) player_move(p1, UP);

    #ifndef VERTICAL_POINTS
        if (p1->sprite.x+p1->sprite.w <= game.width/2) {
            key_pressed(SDL_SCANCODE_D) player_move(p1, RIGHT);
        }
        key_pressed(SDL_SCANCODE_S) player_move(p1, DOWN);
    #else
        if (p1->sprite.y+p1->sprite.h < game.height/2) {
            key_pressed(SDL_SCANCODE_S) player_move(p1, DOWN);
        }
        key_pressed(SDL_SCANCODE_D) player_move(p1, RIGHT);
    #endif
    
    key_pressed(SDL_SCANCODE_A) player_move(p1, LEFT);
    
    
    
    
    // p2

    #ifndef VERTICAL_POINTS
        key_pressed(SDL_SCANCODE_I) player_move(p2, UP);
        if (p2->sprite.x >= game.width/2) {
            key_pressed(SDL_SCANCODE_J) player_move(p2, LEFT);
        }
    #else
        if (p2->sprite.y > game.height/2) {
            key_pressed(SDL_SCANCODE_I) player_move(p2, UP);
        }
        key_pressed(SDL_SCANCODE_J) player_move(p2, LEFT);

    #endif
    
    key_pressed(SDL_SCANCODE_L) player_move(p2, RIGHT);
    key_pressed(SDL_SCANCODE_K) player_move(p2, DOWN);
 
    
    if (game.key_states[SDL_SCANCODE_Q]) 
    game.running = false;
    
    key_pressed(SDL_SCANCODE_R) {
        gameplay_reset();
        game_refresh_score();
    }
    
    
    
    balls_move();
    player_ball_colission();
}  


static void game_gameplay() {
    
    gameplay_events();
    
    ball_draw();
    players_draw();
    
    game_display_score();
}

int main(int argc, char *argv[]) {
    
    srand(time(NULL));
    
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        sdlerror:
        panic("%s", SDL_GetError());
    }


    if (!SDL_CreateWindowAndRenderer("slopp ass game", DEFAULT_WIDTH, DEFAULT_HEIGHT, 0, &game.window, &game.render)) {
        SDL_Quit();
        goto sdlerror;
    }

    if (!TTF_Init()) {
        SDL_Quit();
        goto sdlerror;
    }
    
    game_setup();
    game_startscreen_setup();
    game.running = 1;
    game.key_states = (const unsigned char *)SDL_GetKeyboardState(NULL);
    
    while (game.running) {
        uint64_t start = getTimeInNano();
        SDL_SetRenderDrawColor(game.render, sdl_color(BACKGROUND));
        SDL_RenderClear(game.render);
        
        eventsHandler();

        switch (game.current_screen)
        {
        case SCREEN_INTRO:
            if (SKIP_INTRO) {
                game.current_screen = SCREEN_GAME;
                break;
            }
            game_startscreen();    
            break;
        case SCREEN_GAME:
            game_gameplay();
            break;
        default:
            break;
        }
        



        SDL_RenderPresent(game.render);

        uint64_t end = getTimeInNano();
        uint64_t delta = end - start;
        if (delta < DELTA_TIME) {
            usleep((DELTA_TIME - delta) / 1000);
        }
    }
 
    game_destroy();

    SDL_Quit();
    return 0;
}
