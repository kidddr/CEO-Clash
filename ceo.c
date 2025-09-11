/* TO-DO
- usar a info das anchors pra determinar a distancia do caminhar
- resolver posicionamentos quando dir < 0
- re-implementar colisões com as novas hitboxes
- aplicar hurtboxes
- Jogar *todos* os dados no data.txt
    - jump power
    - dano dos ataques
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <SDL.h>
#include <SDL_image.h>
#include "basics.h"
#include "vec2d.h"
#include "transform.h"
#include "cvec.h"

#define buflen 512
char buf [buflen];


float WALL_L = 0;
float WALL_R = 0;
float FLOOR_Y = 0;

// STATES
enum{ IDLE, WALK, JUMP, FALL, LAND, CROUCH, ATTACK };


typedef struct {

    SDL_Texture *spritesheet;

    SDL_FRect  *srcs;
    vec2d *anchors;
    SDL_FRect **hitboxes;
    SDL_FRect **hurtboxes;

    int state;
    int *state_frame_offsets; // aonde comeca os frames desse estado
    int frame;

    int direcao;

    vec2d pos;
    vec2d vel;

    bool no_controle;// true = estou no controle do meu corpo, 
                     // false = meu corpo foi arremessado por forcas externas

    float walkspeed;
    float jumppower;

} Fighter;


void Fighter_load_spritesheet( SDL_Renderer *R, Fighter *F, char *filename ){

    SDL_snprintf( buf, buflen, "%s.png", filename );
    F->spritesheet = IMG_LoadTexture(R, buf );
    SDL_snprintf( buf, buflen, "%s.txt", filename );
    SDL_IOStream *d = SDL_IOFromFile( buf, "r" );
    if( d != NULL ){

        F->srcs = NULL;
        F->anchors = NULL;
        F->hitboxes = NULL;
        F->hurtboxes = NULL;
        F->state_frame_offsets = NULL;

        char name [64];
        name[0] = '\0';

        int line = 0;
        while( SDL_GetIOStatus(d) == SDL_IO_STATUS_READY ){

            fscan_str_until( d, buf, 64, "[" );
            if( SDL_GetIOStatus(d) != SDL_IO_STATUS_READY ) break;
            if( SDL_strcmp( name, buf ) != 0 ){
                vector_push( F->state_frame_offsets, line );
            }
            SDL_strlcpy( name, buf, 64 );

            char tags [6][24] = { "\n", "src:", "foot:", "anchor:", "hitbox:", "hurtbox:" };
            struct tag_data td = tag_finder( d, tags, 6, 0 );

            vector_push( F->srcs, ((SDL_FRect){-1,-1,-1,-1}) );
            vector_push( F->anchors, v2d(-1,-1) );
            vector_push( F->hitboxes, NULL );
            vector_push( F->hurtboxes, NULL );

            //SDL_Log(">%d tags\n", td.length );

            for (int i = 0; i < td.length; ++i){

                //SDL_Log("[%d]: %s\n", i, tags[ td.indices[i] ] );
                SDL_SeekIO( d, td.locations[i], SDL_IO_SEEK_SET );
                fscan_str_until_any( d, buf, buflen, ":\n" );
                //SDL_Log("buf: %s\n", buf );

                switch( td.indices[i] ){
                    case 1:{ // src:
                        int x, y, w, h;
                        int matches = SDL_sscanf( buf, "%d, %d, %d, %d", &x, &y, &w, &h );
                        //SDL_Log("matches: %d\n", matches );
                        if( matches == 4 ){
                            F->srcs[line] = (SDL_FRect){x, y, w, h};
                        } else SDL_Log( "Bad src matches!" );
                    } break;
                    case 2:{ // foot:
                        int x, y;
                        int matches = SDL_sscanf( buf, "%d, %d", &x, &y );
                        //SDL_Log("matches: %d\n", matches );
                        if( matches == 2 ){
                            F->anchors[line] = v2d(x, y);
                        } else SDL_Log( "Bad foot matches!" );
                    } break;
                    case 3:{ // anchor:
                        
                    } break;
                    case 4:{ // hitbox:
                        int x, y, w, h;
                        int matches = SDL_sscanf( buf, "%d, %d, %d, %d",  &x, &y, &w, &h );
                        if( matches == 4 ){
                            vector_push( F->hitboxes[line], ((SDL_FRect){x, y, w, h}) );
                        } else SDL_Log( "Bad hitbox matches!" );
                    } break;
                    case 5:{ // hurtbox:
                        int x, y, w, h;
                        int matches = SDL_sscanf( buf, "%d, %d, %d, %d",  &x, &y, &w, &h );
                        if( matches == 4 ){
                            vector_push( F->hurtboxes[line], ((SDL_FRect){x, y, w, h}) );
                        } else SDL_Log( "Bad hurtbox matches!" );
                    } break;
                }
            }

            line++;
            free_tag_data( &td );
        }

        SDL_CloseIO( d );

        vector_push( F->state_frame_offsets, line );
        int states_N = vector_size( F->state_frame_offsets );
        for (int i = 0; i < states_N; ++i ){
            SDL_Log("F->state_frame_offsets[%d]: %d\n", i, F->state_frame_offsets[i] );
        }
    }
    else{
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load \"%s\": %s.", filename, SDL_GetError());
    }
}

bool Fighter_no_ar( Fighter *F ){
    return F->state == JUMP || F->state == FALL;
}

void Fighter_control( Fighter *F, bool cu, bool cd, bool cl, bool cr, bool cA ){

    vec2d desloc = v2dzero;

    // can we jump or attack?
    if( F->state == IDLE || F->state == WALK ){
        if( cu ){
            if( !Fighter_no_ar(F) ){// previne double jump
                F->vel = v2d( 0, F->jumppower );
                F->no_controle = true;
                F->state = JUMP; F->frame = 0;
            }
        }
        if( cA && !Fighter_no_ar(F) ){
            F->state = ATTACK; F->frame = 0;
        }
    }



    switch( F->state ){

        case IDLE:{
            if( cl || cr ){
                F->state = WALK; F->frame = 0;
            }
        } break;

        case WALK:{
            
            if( cl || cr ){
                desloc.x = (cr-cl) * F->walkspeed;
            }
            else{
                F->state = IDLE; F->frame = 0;
            }

        } break;

        case JUMP:{
            if( F->no_controle && (cr-cl) != 0 ){
                desloc.x = (cr-cl) * F->walkspeed;
            }
            F->vel.y += 1;

            if( F->vel.y > 0 ){
                F->state = FALL; F->frame = 0;
            }
            
        } break;

        case FALL:

            if( F->no_controle && (cr-cl) != 0 ){
                desloc.x = (cr-cl) * F->walkspeed;
            }
            F->vel.y += 1;
    
            break;

        case LAND:
    
            break;

        case CROUCH:
            if(cd){
            F->state = CROUCH; F->frame = 0;
        }

            break;

        case ATTACK:
    
            break;
    }
    

    if( desloc.x != 0 ){// no chao
        F->pos.x += desloc.x;
        F->direcao = (desloc.x > 0)-(desloc.x < 0);
    }

    F->pos.y += F->vel.y;

    if( F->pos.y > FLOOR_Y ){
        F->pos.y = FLOOR_Y;
        F->vel.y = 0;
        F->state = LAND; F->frame = 0;
    }

    if( F->pos.x < WALL_L ){// colisao com a parede Left
        F->pos.x = WALL_L;
    }
    if( F->pos.x > WALL_R ){// colisao com a parede Right
        F->pos.x = WALL_R;
    }

    /*
    SDL_FRect *hbs = F->hitboxes[ F->state_frame_offsets[ F->state ] + F->frame ];
    int hn = vector_size( hbs );
    for (int h = 0; h < hn; ++h ){
        SDL_FRect *hitbox = hbs + h;

        if( F->pos.x - 0.5 * hitbox->w < WALL_L ){// colisao com a parede Left
            F->pos.x = WALL_L + 0.5 * hitbox->w;
        }
        if( F->pos.x + 0.5 * hitbox->w > WALL_R ){// colisao com a parede Right
            F->pos.x = WALL_R - 0.5 * hitbox->w;
        }
    }
    //hitbox->x = F->pos.x - 0.5 * hitbox->w;
    //hitbox->y = F->pos.y - hitbox->h;
    */
}

void display_Fighter( SDL_Renderer *R, Fighter *F ){
    int flip = SDL_FLIP_NONE;
    if( F->direcao < 0 ) flip = SDL_FLIP_HORIZONTAL;
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    //SDL_Log( "%p: %d, %d", F, F->state, frm );
    SDL_FRect dst = (SDL_FRect){ F->pos.x - F->anchors[frm].x, F->pos.y - F->anchors[frm].y, 
                                 F->srcs[frm].w, F->srcs[frm].h };
    SDL_RenderTextureRotated( R, F->spritesheet, F->srcs + frm, &dst, 0, NULL, flip );
}


void Fighter_tick_frame( Fighter *F ){

    F->frame += 1;

    if( F->state == ATTACK ) SDL_Log( "%d - %d - %d", F->frame, F->state_frame_offsets[ F->state + 1 ], F->state_frame_offsets[ F->state ] );
    if( F->frame >= F->state_frame_offsets[ F->state + 1 ] - F->state_frame_offsets[ F->state ] ){
        
        switch( F->state ){
            case IDLE:
            case CROUCH:
            case WALK:// Loop animation
                F->frame = 0;
                break;

            case LAND:
            case ATTACK:// finish animation
                F->state = IDLE; F->frame = 0;
                break;

            default:// just stay in the last frame
                F->frame -= 1;
                break;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~O~~~~~~~~~~| M A I N |~~~~~~~~~~~O~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int main(int argc, char *argv[]){

    SDL_Window *window;
    SDL_Renderer *R;
    int width = 600;
    int height = 400;
    int cx, cy;
    int c2x, c2y;
    int loop = 1;


    if( !SDL_Init(SDL_INIT_VIDEO) ){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 3;
    }
    if( !SDL_CreateWindowAndRenderer( "CEO_Clash", width, height, 
                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, 
                                      &window, &R ) ){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
        return 3;
    }
    SDL_GetWindowSize( window, &width, &height );
    cx = width / 2;
    cy = height / 2;

    WALL_R = width;
    FLOOR_Y = height - 100;

    SDL_srand(0);



    //TEXTURAS

    SDL_Texture *Fundo = IMG_LoadTexture(R,"Assets/ceu1.png");
    float fundow, fundoh;
    SDL_GetTextureSize(Fundo, &fundow, &fundoh);

    SDL_Texture *Fundo2 = IMG_LoadTexture(R,"Assets/ceu2.png");
    float fundo2w, fundo2h;
    SDL_GetTextureSize(Fundo2, &fundo2w, &fundo2h);

    SDL_Texture *Fundo3 = IMG_LoadTexture(R,"Assets/morros3.png");
    float fundo3w, fundo3h;
    SDL_GetTextureSize(Fundo3, &fundo3w, &fundo3h);

    SDL_Texture *Fundo4 = IMG_LoadTexture(R,"Assets/pred4.png");
    float fundo4w, fundo4h;
    SDL_GetTextureSize(Fundo4, &fundo4w, &fundo4h);

    SDL_Texture *Fundo5 = IMG_LoadTexture(R,"Assets/pred5.png");
    float fundo5w, fundo5h;
    SDL_GetTextureSize(Fundo5, &fundo5w, &fundo5h);
    



     /*TEXTURAS PERSONAGENS

    SDL_Texture *Piwis = IMG_LoadTexture(R, "Assets/Pinto_outline.png");
    float fw, fh;
    SDL_GetTextureSize(Piwis, &fw, &fh);

    Transform T = (Transform){256,256,cx,cy,1,1};
    int scaleI = 0;

    SDL_Texture *Piwispez = IMG_LoadTexture(R, "Assets/piwispez.png");
    SDL_GetTextureSize(Piwispez, &fw, &fh);



    SDL_Texture *Melon = IMG_LoadTexture(R, "Assets/Sus.png");
    float cyberw, cyberh;
    SDL_GetTextureSize(Melon, &cyberw, &cyberh);

    Transform I = (Transform){256,256,c2x,c2y,1,1};
    int scaleT = 0;
    
    SDL_Texture *pat1 = IMG_LoadTexture(R,"Assets/piwisAT1.png");
    float pat1w, pat1h;
    SDL_GetTextureSize(pat1, &pat1w, &pat1h);*/




    SDL_Texture *Fundo6 = IMG_LoadTexture(R,"Assets/pred6.png");
    float fundo6w, fundo6h;
    SDL_GetTextureSize(Fundo6, &fundo6w, &fundo6h);




    //  HITBOXES PERSONAGENS


    Fighter P1 = {0};
    Fighter_load_spritesheet( R, &P1, "Assets/Susk_Sprites" );
    P1.pos = v2d( 100, FLOOR_Y );
    P1.walkspeed = 8;
    P1.jumppower = -24;
    P1.direcao = 0;

    Fighter P2 = {0};


    Fighter_load_spritesheet( R, &P2, "Assets/venom abr2" );
    P2.pos = v2d( width-100, FLOOR_Y );
    P2.walkspeed = 5;
    P2.jumppower = -30;
    P2.direcao = 1;


    bool p1u = 0, p1d = 0, p1l = 0, p1r = 0, p1_A = 0;// up down left right
    bool p2u = 0, p2d = 0, p2l = 0, p2r = 0, p2_A = 0;

    
    int frame_period = SDL_lround( 1000 / 60.0 );
    int animation_period = SDL_lround( 1000 / 10.0 );
    Uint64 next_ani_tick = SDL_GetTicks() + animation_period;


    SDL_Log("<<Entering Loop>>");
    while ( loop ) { //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> L O O P <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 
        
        SDL_Event event;
        while( SDL_PollEvent(&event) ){
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    loop = 0;
                    break;
                case SDL_EVENT_KEY_DOWN:
                         if( event.key.key == 'w' ) p1u = 1;
                    else if( event.key.key == 's' ) p1d = 1;
                    else if( event.key.key == 'a' ) p1l = 1;
                    else if( event.key.key == 'd' ) p1r = 1;
                    else if( event.key.key == 'e' ) p1_A = 1;

                    else if( event.key.key == SDLK_UP    ) p2u = 1;
                    else if( event.key.key == SDLK_DOWN  ) p2d = 1;
                    else if( event.key.key == SDLK_LEFT  ) p2l = 1;
                    else if( event.key.key == SDLK_RIGHT ) p2r = 1;
                    else if( event.key.key == 'm' ) p2_A = 1;
                    /*else if( event.key.key == SDLK_SPACE ) {
                         if (!P1.atacando && P1.ataque_cooldown == 0) {
                              P1.atacando = true;
                              P1.ataque_timer = 10; // duração do ataque em frames
                            }
                    }
                    else if( event.key.key == SDLK_RETURN ) {
                         if (!P2.atacando && P2.ataque_cooldown == 0) {
                              P2.atacando = true;
                              P2.ataque_timer = 10;
                            }
                    }*/
                    break;
                case SDL_EVENT_KEY_UP:
                         if( event.key.key == 'w' ) p1u = 0;
                    else if( event.key.key == 's' ) p1d = 0;
                    else if( event.key.key == 'a' ) p1l = 0;
                    else if( event.key.key == 'd' ) p1r = 0;
                    else if( event.key.key == 'e' ) p1_A = 0;

                    else if( event.key.key == SDLK_UP    ) p2u = 0;
                    else if( event.key.key == SDLK_DOWN  ) p2d = 0;
                    else if( event.key.key == SDLK_LEFT  ) p2l = 0;
                    else if( event.key.key == SDLK_RIGHT ) p2r = 0;
                    else if( event.key.key == 'm' ) p2_A = 0;
                    break;
            }
        }

        SDL_SetRenderDrawColor( R, 200,200,200,255 );
        SDL_RenderClear(R);

        //SDL_SetRenderDrawColor( R, 0,0,0,255 );
        //SDL_RenderLine( R, 0, FLOOR_Y, width, FLOOR_Y );

        SDL_FRect fundodest = {0,0,width,height};
        SDL_RenderTexture(R, Fundo, NULL, &fundodest);

        SDL_FRect fundo2dest = {0,0,width,height};
        SDL_RenderTexture(R, Fundo2, NULL, &fundo2dest);

        SDL_FRect fundo3dest = {0,0,width,height};
        SDL_RenderTexture(R, Fundo3, NULL, &fundo3dest);

        SDL_FRect fundo4dest = {0,0,width,height};
        SDL_RenderTexture(R, Fundo4, NULL, &fundo4dest);

        SDL_FRect fundo5dest = {0,0,width,height};
        SDL_RenderTexture(R, Fundo5, NULL, &fundo5dest);


        Fighter_control( &P1, p1u, p1d, p1l, p1r, p1_A );

        Fighter_control( &P2, p2u, p2d, p2l, p2r, p2_A );
        
        /*
        // colisao entre os Fighters
        if( SDL_FRect_overlap( &(P1.hitbox), &(P2.hitbox) ) ){
            Fighter *LF;
            Fighter *RF;
            if( P1.pos.x < P2.pos.x ){
                LF = &P1;
                RF = &P2;
            } else {
                LF = &P2;
                RF = &P1;
            }
            int overlap = (LF->hitbox.x + LF->hitbox.w)-(RF->hitbox.x);
            LF->pos.x -= 0.5 * overlap;
            RF->pos.x += 0.5 * overlap;
            LF->hitbox.x = LF->pos.x - 0.5 * LF->hitbox.w;
            RF->hitbox.x = RF->pos.x - 0.5 * RF->hitbox.w;
        }
        */
        if( SDL_GetTicks() >= next_ani_tick ){
            Fighter_tick_frame( &P1 );
            next_ani_tick = SDL_GetTicks() + animation_period;

            Fighter_tick_frame( &P2 );
            next_ani_tick = SDL_GetTicks() + animation_period;
        }

        display_Fighter( R, &P1 );
        display_Fighter( R, &P2 );

        /*if( SDL_GetTicks() >= next_ani_tick ){
            Fighter_tick_frame( &P1 );
            next_ani_tick = SDL_GetTicks() + animation_period;
        }
        display_Fighter( R, &P1 );*/

        if( SDL_GetTicks() >= next_ani_tick ){
            Fighter_tick_frame( &P2 );
            next_ani_tick = SDL_GetTicks() + animation_period;
        }
        display_Fighter( R, &P2 );

        //int flip2;
        //if( P2.direcao > 0 ) flip2 = 1;
        //else if( P2.direcao < 0 ) flip2 = 0;
        //SDL_RenderTextureRotated(R, Melon, NULL, &(P2.hitbox), 0, NULL, flip2);
    
        SDL_FRect fundo6dest = {0,0,width,height};
        SDL_RenderTexture(R, Fundo6, NULL, &fundo6dest);

        
        SDL_RenderPresent(R);
        SDL_framerateDelay( frame_period );

    }//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> / L O O P <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

