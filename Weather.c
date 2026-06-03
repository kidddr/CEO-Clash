#include "Weather.h"


void init_Weather( Weather_ctx *W, int width, int height ){
	/* set up the data for a bunch of points. */
    for (int i = 0; i < NUM_FLAKES; i++) {
        W->points[i].x = SDL_randf() * ((float) width);
        W->points[i].y = SDL_randf() * ((float) height);
        W->point_speeds[i] = MIN_PIXELS_PER_SECOND + (SDL_randf() * (MAX_PIXELS_PER_SECOND - MIN_PIXELS_PER_SECOND));
    }
}



void draw_Weather( SDL_Renderer *R, Weather_ctx *W, int width, int height ){

	static Uint64 last_time = 0;

	const Uint64 now = SDL_GetTicks();
    const float elapsed = ((float) (now - last_time)) / 1000.0f;  /* seconds since last iteration */
    int i;

    /* let's move all our points a little for a new frame. */
    for (i = 0; i < SDL_arraysize(W->points); i++) {
        const float distance = elapsed * W->point_speeds[i];
        W->points[i].x += distance;
        W->points[i].y += distance;
        if ((W->points[i].x >= width) || (W->points[i].y >= height)) {
            /* off the screen; restart it elsewhere! */
            if (SDL_rand(2)) {
                W->points[i].x = SDL_randf() * ((float) width);
                W->points[i].y = 0.0f;
            } else {
                W->points[i].x = 0.0f;
                W->points[i].y = SDL_randf() * ((float) height);
            }
            W->point_speeds[i] = MIN_PIXELS_PER_SECOND + (SDL_randf() * (MAX_PIXELS_PER_SECOND - MIN_PIXELS_PER_SECOND));
        }
    }

    last_time = now;

    SDL_SetRenderDrawColor(R, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */
    SDL_RenderPoints(R, W->points, SDL_arraysize(W->points));  /* draw all the W->points! */
}