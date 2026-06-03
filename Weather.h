#ifndef WEATHER_H_INCLUDED
#define WEATHER_H_INCLUDED

#include "basics.h"

#define NUM_FLAKES 1500
#define MIN_PIXELS_PER_SECOND 130  /* move at least this many pixels per second. */
#define MAX_PIXELS_PER_SECOND 260  /* move this many pixels per second at most. */


typedef struct weather_struct{
	SDL_FPoint points[NUM_FLAKES];
	float point_speeds[NUM_FLAKES];
} Weather_ctx;

void init_Weather( Weather_ctx *W, int width, int height );
void draw_Weather( SDL_Renderer *R, Weather_ctx *W, int width, int height );


#endif