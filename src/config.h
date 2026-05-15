#ifndef GAME_CONFIG_H_
#define GAME_CONFIG_H_

#include "definitions.h" 

static const Color colors[] = {
    color(0, 0, 0, 0xff),          // black (reserved for background)
    color(0xff, 0, 0, 0xff),       // red 
    color(0, 0xff, 0, 0xff),       // green
    color(0, 0, 0xff, 0xff),       // blue
    color(0xff, 0xff, 0, 0xff),    // yellow
    color(0xdd, 0xaa, 0xcc, 0xff), //
    color(0xcc, 0xcc, 0xcc, 0xff), // gray
    color(0x18, 0x18, 0x18, 0xff), // semi-black
    color(0xAA, 0xDD, 0, 0xff),    //
    color(0xff, 0xff, 0xff, 0xff), // white
};

#define BACKGROUND colors[0]
#define RED colors[1]
#define GREEN colors[2]
#define BLUE  colors[3]
#define YELLOW colors[4]
#define WHITE colors[9]



#define FPS 60 
#define SKIP_INTRO 1
#define PLAYERS_VELOCITY 10.0f
#define PLAYERS_W w/10.0f
#define PLAYERS_H h/2.0f
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define BALLS_MAX_SPEED 10
#define BALL_QUANTITY 1

#define PLAYER_1_COLOR WHITE
#define PLAYER_2_COLOR WHITE
#define BALLS_COLOR color_pick_random() 

#define AUDIO_FILE "fumo.wav"

#endif