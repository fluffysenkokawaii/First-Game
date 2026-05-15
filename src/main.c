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
    SDL_PumpEvents();
}

// static inline unsigned char collision_box(SDL_FRect a, SDL_FRect b) {
//     return (a.x < b.x+b.w   && // crossing X
//             a.x + a.w > b.x && // ----------
//             a.y < b.y+b.h   && // crossing Y
//             a.y+a.h > b.y);    // ----------
// }

static inline unsigned char collision_direction(SDL_FRect r1, SDL_FRect r2, Directions2d *dirs, unsigned char reverse) {
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
            dirs->y = UP + reverse;
            ret += 1;
        } else if (higher < bottom && bottom < downer) {
            dirs->y = DOWN - reverse;
            ret += 2;
        }
    }

    if ((top < higher && higher < bottom) || (higher < top && top < downer)) {
        if (xdot < righter && righter < xdotfinnish) {
            dirs->x = RIGHT - reverse;
            ret += 4; 
        } else if (xdot < lefter && lefter < xdotfinnish) {
            dirs->x = LEFT + reverse;
            ret += 8;
        } 
    }
    return ret;
} 

static void game_create_balls() {
    for (size_t i = 0; i < BALL_QUANTITY; i++ ) {
        game.balls[i] = (Ball) { 
            .sprite = (SDL_FRect) {.w = 50, .h = 50, .x=game.width/2-50, .y=game.height/2-50 },
            .color = BALLS_COLOR,
            .dirs = {direction_pick_randomX(), direction_pick_randomY()},
            .velocity = {pick_random(1, 5), pick_random(1, 5)}
        };
    }
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
    int w, h = 0;
    SDL_GetWindowSize(game.window, &w, &h);
    game.width = w;
    game.height = h;
    float pw = PLAYERS_W;
    float ph = PLAYERS_H;

    game.players[0] = (Player){
        .life = 100,
        .dir = FREEZE,
        .sprite = (SDL_FRect) {.w = pw, .h = ph, .x = w/100, .y = h/100},
        .color = PLAYER_1_COLOR,
        .velocity = PLAYERS_VELOCITY,
        .points = 0,
    };
    game.players[1] = (Player){
        .life = 100,
        .dir = FREEZE,
        .sprite = (SDL_FRect) {.w = pw, .h = ph, .x = w-200 , .y = h-200},
        .color = PLAYER_2_COLOR,
        .velocity = PLAYERS_VELOCITY,
        .points = 0,
    };
    game_create_balls();
  
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

static void ball_move() {

    const SDL_FRect game_borders = (SDL_FRect){.x = 0, .y = 0, .w = game.width, .h = game.height };

    for (int i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i];

        // Borders collision
        uint8_t detect = collision_direction(game_borders, ball->sprite, &ball->dirs, 1);

        if (detect & (COLLISION_LEFT | COLLISION_RIGHT)) {
            if (ball->dirs.x != RIGHT) {
                game.players[0].points += 1;

            } else {
                game.players[1].points += 1;
            }
            ball_reset(ball);
            game_refresh_score();
            game_sound_score();
        }
        
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
}

static void player_check_ball_colission() {
    for (size_t i = 0; i < length(game.balls); i++) {
        Ball *ball = &game.balls[i];
        for (size_t j = 0; j < length(game.players); j++) {
            Player * player = &game.players[j];
            int direction = 0;
            if ((direction = collision_direction(player->sprite, ball->sprite, &game.balls->dirs, 0))) {
                if (direction & COLLISION_LEFT) {
                    ball->sprite.x = player->sprite.x-ball->sprite.w;
                }
                if (direction & COLLISION_RIGHT) {
                    ball->sprite.x = player->sprite.x+player->sprite.w;
                }
                if (direction & COLLISION_BOTTOM) {
                    ball->sprite.y = player->sprite.y+player->sprite.h;
                }
                if (direction & COLLISION_TOP) {
                    ball->sprite.y = player->sprite.y-ball->sprite.h;
                }
                game.balls->velocity[0] += 1; 
                game.balls->velocity[1] += 1;
                if (game.balls->velocity[0] > BALLS_MAX_SPEED) {
                    game.balls->velocity[0] = BALLS_MAX_SPEED;
                }
                if (game.balls->velocity[1] > BALLS_MAX_SPEED) {
                    game.balls->velocity[1] = BALLS_MAX_SPEED;
                }
            };
        }
    }
}

void game_display_score() {
    SDL_RenderTexture(game.render, game.score, NULL, &(SDL_FRect){10, 10, game.width/2, game.height/10});
}

void game_reset() {

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
}

static uint64_t timer_general = 0; 

static unsigned char game_startscreen() {

    if (game.key_states[SDL_SCANCODE_Q]) {
        game.running = false;
        return 1;
    }

    uint64_t time = timer_general/FPS;
    if (time > 9) return 0;

    int mx = game.width/2;
    int my = game.height/2;

    game_display_text(5, mx, my+80);
    game_display_text(6, mx*1.5, my+64+64);
    game_display_text(7, mx*1.5, my+64+64+32);

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
    timer_general++;
    return 1;
}

static void game_gameplay() {
    players_move_event();
    ball_move();
    player_check_ball_colission();
    

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

        if (SKIP_INTRO || !game_startscreen()) {
            game_gameplay();
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