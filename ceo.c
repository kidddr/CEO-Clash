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

#define GetTexSz(T) float tw, th; \
    SDL_GetTextureSize(T, &tw, &th);

float WALL_L = 0;
float WALL_R = 0;
float FLOOR_Y = 0;

Transform T;

// STATES
#define NUM_STATES 8
enum{ IDLE, WALK, JUMP, FALL, LAND, CROUCH, ATTACK, BEATEN };
const char state_names [NUM_STATES][24] = { "idle", "walk", "jump", "fall", "land", "crouch", "attack", "beaten" };


typedef struct {

    SDL_Texture **frames;
    vec2d *anchors;
    SDL_FRect **hitboxes; // Onde eu RECEBO hits
    SDL_FRect **hurtboxes; // onde eu MACHUCO os outros

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


void Load_fighter( SDL_Renderer *R, Fighter *F, char *path ){


    SDL_snprintf( buf, buflen, "%s/data.txt", path );
    SDL_IOStream *d = SDL_IOFromFile( buf, "r" );
    

    if( d != NULL ){

        F->frames = NULL;
        F->anchors = NULL;
        F->hitboxes = NULL;
        F->hurtboxes = NULL;

        struct tag_data std = tag_finder( d, state_names, NUM_STATES, -1 );

        int state_counts [NUM_STATES];
        SDL_memset4( state_counts, 0, NUM_STATES );
        for (int S = 0; S < std.length; ++S){
            if( std.indices[S] < 0 || std.indices[S] >= NUM_STATES ){
                SDL_Log( "???? huh??" );
                break;
            }
            state_counts[ std.indices[S] ] += 1;
        }

        F->state_frame_offsets = SDL_malloc( (NUM_STATES + 1) * sizeof(int) );
        F->state_frame_offsets[0] = 0;
        for (int s = 1; s < NUM_STATES; ++s ){
            F->state_frame_offsets[s] = F->state_frame_offsets[s-1] + state_counts[s-1];
            //SDL_Log( "sfo[%d] = %d", s, F->state_frame_offsets[s] );
        }
        F->state_frame_offsets[NUM_STATES] = F->state_frame_offsets[NUM_STATES-1] + state_counts[NUM_STATES-1];

        int line = 0;
        //while( SDL_GetIOStatus(d) == SDL_IO_STATUS_READY ){
        for (int S = 0; S < NUM_STATES; ++S ){
            for (int L = 0; L < std.length; ++L){

            	if( std.indices[L] != S ) continue;

            	//SDL_Log( "S:%d L:%d line:%d", S, L, line );

                SDL_SeekIO( d, std.locations[L], SDL_IO_SEEK_SET );
                //if( SDL_GetIOStatus(d) != SDL_IO_STATUS_READY ) break;

                const char tags [5][24] = { "\n", "file:", "anchor:", "hitbox:", "hurtbox:" };
                struct tag_data td = tag_finder( d, tags, 6, 0 );

                vector_push( F->frames, NULL );
                vector_push( F->anchors, v2d(-1,-1) );
                vector_push( F->hitboxes, NULL );
                vector_push( F->hurtboxes, NULL );

                //SDL_Log(">%d tags\n", td.length );

                for (int i = 0; i < td.length; ++i){

                    //SDL_Log("[%d]: %s\n", i, tags[ td.indices[i] ] );
                    SDL_SeekIO( d, td.locations[i], SDL_IO_SEEK_SET );
                    fscan_str_until_any( d, buf, buflen, ":;\n" );
                    //SDL_Log("buf: %s\n", buf );

                    switch( td.indices[i] ){
                        case 1:{ // file:
                            char pathbuf [512];
                            SDL_snprintf( pathbuf, 512, "%s/%s", path, buf );
                            F->frames[line] = IMG_LoadTexture( R, pathbuf );
                            /*
                                int x, y, w, h;
                                int matches = SDL_sscanf( buf, "%d, %d, %d, %d", &x, &y, &w, &h );
                                //SDL_Log("matches: %d\n", matches );
                                if( matches == 4 ){
                                    F->frames[line] = (SDL_FRect){x, y, w, h};
                                } else SDL_Log( "Bad src matches!" );
                                */
                        } break;
                        case 2:{ // anchor:
                            int x, y;
                            int matches = SDL_sscanf( buf, "%d, %d", &x, &y );
                            //SDL_Log("matches: %d\n", matches );
                            if( matches == 2 ){
                                F->anchors[line] = v2d(x, y);
                            } else SDL_Log( "Bad foot matches!" );
                        } break;
                        case 3:{ // hitbox:
                            int x, y, w, h;
                            int matches = SDL_sscanf( buf, "%d, %d, %d, %d",  &x, &y, &w, &h );
                            if( matches == 4 ){
                                vector_push( F->hitboxes[line], ((SDL_FRect){x, y, w, h}) );
                            } else SDL_Log( "Bad hitbox matches!" );
                        } break;
                        case 4:{ // hurtbox:
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
        }

        free_tag_data( &std );

        SDL_CloseIO( d );
    }
    else{
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load \"%s\": %s.", path, SDL_GetError());
    }
}

bool Fighter_no_ar( Fighter *F ){
    return F->state == JUMP || F->state == FALL;
}

void Fighter_control( Fighter *F, bool cu, bool cd, bool cl, bool cr, bool cA ){

    vec2d desloc = v2dzero;

    if(cr && cl){
        cr = false;
        cl = false;
    }

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
        if( cd && !Fighter_no_ar(F) ){
            F->state = CROUCH; F->frame = 0;
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
            
            break;

        case ATTACK:
    
            break;

        case BEATEN:
            
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

SDL_FRect fighter_box_to_world( Fighter *F, SDL_FRect *box ){
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    SDL_FRect out = *box;
    if( F->direcao < 0 ){
        out.x = F->pos.x + (2 * F->anchors[frm].x) - box->x - box->w;
    } else{
        out.x = F->pos.x - F->anchors[frm].x + box->x; 
    }
    out.y = F->pos.y - F->anchors[frm].y + box->y;
    return out;
}

SDL_FRect get_Fighter_dstrct_now( Fighter *F ){
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    GetTexSz(F->frames[frm]);
    SDL_FRect dst = (SDL_FRect){ 0, F->pos.y - F->anchors[frm].y, tw, th };
    if( F->direcao < 0 ){
        dst.x = F->pos.x + (2 * F->anchors[frm].x) - tw;
    } else {
        dst.x = F->pos.x - F->anchors[frm].x;
    }
    return dst;
}

SDL_FRect get_Fighter_boundingbox_now( Fighter *F ){
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    int hs = vector_size( F->hitboxes[frm] );
    SDL_FRect box = fighter_box_to_world( F, F->hitboxes[frm]+0 );
    for (int i = 1; i < hs; ++i){
        box = add_rects( &box, F->hitboxes[frm]+i );
    }
    return box;
}

void fighters_hurt( Fighter *attacker, Fighter *defender ){
    int Afrm = attacker->state_frame_offsets[ attacker->state ] + attacker->frame;
    int Dfrm = defender->state_frame_offsets[ defender->state ] + defender->frame;
    int hurts = vector_size( attacker->hurtboxes[Afrm] );
    int hits = vector_size( defender->hitboxes[Dfrm] );
    for (int a = 0; a < hurts; ++a) {
        for( int d = 0; d < hits; ++d ){
            SDL_FRect abox = fighter_box_to_world( attacker, attacker->hurtboxes[Afrm] + a );
            SDL_FRect dbox = fighter_box_to_world( defender, defender->hitboxes[Dfrm] + d );
            if( SDL_FRect_overlap( &abox, &dbox ) ){
                // acertou o ataque!!!
                //defender->hp -= 5;
                defender->state = BEATEN; defender->frame = 0;
                SDL_Log("Ai, me bateu!!");

            }
        }
    }
}


void display_Fighter( SDL_Renderer *R, Fighter *F ){
    int flip = SDL_FLIP_NONE;
    if( F->direcao < 0 ) flip = SDL_FLIP_HORIZONTAL;
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    //SDL_Log( "%p: %d, %d", F, F->state, frm );
    SDL_FRect dst = get_Fighter_dstrct_now( F );
    dst = apply_transform_frect( &dst, &T ); // PARA A CÂMERA
    //SDL_Log( "%g, %g, %g, %g", dst.x, dst.y, dst.w, dst.h );
    SDL_RenderTextureRotated( R, F->frames[frm], NULL, &dst, 0, NULL, flip );
}

void display_Fighter_boxes( SDL_Renderer *R, Fighter *F ){
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    int hs = vector_size( F->hitboxes[frm] );

    SDL_SetRenderDrawColor( R, 0, 255, 0, 255 );
    for (int i = 0; i < hs; ++i){
        SDL_FRect rct = (SDL_FRect){ 0, F->pos.y - F->anchors[frm].y + F->hitboxes[frm][i].y,
                                     F->hitboxes[frm][i].w,  F->hitboxes[frm][i].h };
        if( F->direcao < 0 ){
            rct.x = F->pos.x + (2 * F->anchors[frm].x) - F->hitboxes[frm][i].x - F->hitboxes[frm][i].w;
        } else{
            rct.x = F->pos.x - F->anchors[frm].x + F->hitboxes[frm][i].x; 
        }
        SDL_RenderRect( R, &rct );
    }

    hs = vector_size( F->hurtboxes[frm] );
    SDL_SetRenderDrawColor( R, 255, 0, 0, 255 );
    for (int i = 0; i < hs; ++i){
        SDL_FRect rct = (SDL_FRect){ 0, F->pos.y - F->anchors[frm].y + F->hurtboxes[frm][i].y,
                                     F->hurtboxes[frm][i].w,  F->hurtboxes[frm][i].h };
        if( F->direcao < 0 ){
            rct.x = F->pos.x + (2 * F->anchors[frm].x) - F->hurtboxes[frm][i].x - F->hurtboxes[frm][i].w;
        } else{
            rct.x = F->pos.x - F->anchors[frm].x + F->hurtboxes[frm][i].x;

        }
        SDL_RenderRect( R, &rct );
    }
}


void Fighter_tick_frame( Fighter *F ){

    F->frame += 1;
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    //SDL_Log( "%d: %d - %d - %d - %g",F->state, F->frame, F->state_frame_offsets[ F->state + 1 ], F->state_frame_offsets[ F->state ], F->frames[frm].x );
    if( F->frame >= F->state_frame_offsets[ F->state + 1 ] - F->state_frame_offsets[ F->state ] ){
        
        switch( F->state ){
            case IDLE:
            case CROUCH:
                F->state = IDLE; F->frame = 0;
                break;
            case WALK:// Loop animation
                F->frame = 0;
                break;

            case BEATEN:
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

    SDL_srand(0);



    //TEXTURAS

    SDL_Texture *Fundo0 = IMG_LoadTexture(R,"Assets/AdD1.png");
    SDL_Texture *Fundo1 = IMG_LoadTexture(R,"Assets/AdD2.png");
    float fundo0w, fundo0h;
    SDL_GetTextureSize(Fundo0, &fundo0w, &fundo0h);
    SDL_GetTextureSize(Fundo1, &fundo0w, &fundo0h);

    WALL_R = fundo0w;
    FLOOR_Y = height - 100;

    T = (Transform){0,0,0,0,1,1};

    //  HITBOXES PERSONAGENS


    Fighter P1 = {0};
    Load_fighter( R, &P1, "Assets/Susk" );
    P1.pos = v2d( 100, FLOOR_Y );
    P1.walkspeed = 15;
    P1.jumppower = -24;
    P1.direcao = 0;

    Fighter P2 = {0};


    Load_fighter( R, &P2, "Assets/Susk" );
    P2.pos = v2d( width-100, FLOOR_Y );
    P2.walkspeed = 15;
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

        float fighters_dist = SDL_fabsf( P1.pos.x - P2.pos.x );
        fighters_dist *= 1.2;
        if( fighters_dist < width ) T.s = 1;
        else{
            T.s = width / fighters_dist;
        }
        set_scale( &T, T.s );
        float camx = ((P1.pos.x + P2.pos.x) / 2.0f) - (fighters_dist / 2.0f);
        if( camx < 0 ) camx = 0;
        if( camx > fundo0w - fighters_dist ) camx = fundo0w - fighters_dist;
        T.tx = camx;
        float floor_world_y = fundo0h - 100;
        float visible_height = height * T.invs;
        float camy = floor_world_y - FLOOR_Y * T.invs;
        if (camy < 0) camy = 0;
        if (camy > fundo0h - visible_height) camy = fundo0h - visible_height;
        T.ty = FLOOR_Y - (FLOOR_Y * T.invs);

        //if(camy < 0)  ;

        SDL_FRect src_rect = {camx, camy, width * T.invs, height * T.invs};
        SDL_RenderTexture( R, Fundo0, &src_rect, NULL );


        Fighter_control( &P1, p1u, p1d, p1l, p1r, p1_A );

        Fighter_control( &P2, p2u, p2d, p2l, p2r, p2_A );
        
        // colisao entre os Fighters
        SDL_FRect P1bbox = get_Fighter_boundingbox_now( &P1 );
        SDL_FRect P2bbox = get_Fighter_boundingbox_now( &P2 );

        if( SDL_FRect_overlap( &P1bbox, &P2bbox ) ){
            vec2d *LFpos;
            SDL_FRect *LFbox;
            vec2d *RFpos;
            SDL_FRect *RFbox;
            if( P1.pos.x < P2.pos.x ){
                LFbox = &(P1bbox);
                RFbox = &(P2bbox);
                LFpos = &(P1.pos);
                RFpos = &(P2.pos);
            } else {
                LFbox = &(P2bbox);
                RFbox = &(P1bbox);
                LFpos = &(P2.pos);
                RFpos = &(P1.pos);
            }
            float overlap = (LFbox->x + LFbox->w)-(RFbox->x);
            //SDL_Log( "Overlap! (%g + %g) - %g = %g", LFbox->x, LFbox->w, RFbox->x, overlap );
            LFpos->x -= 0.5 * overlap;
            RFpos->x += 0.5 * overlap;
            //LFbox->x = LFpos->x - 0.5 * LFbox->w;
            //RFbox->x = RFpos->x - 0.5 * RFbox->w;
        }


        if( SDL_GetTicks() >= next_ani_tick ){
            Fighter_tick_frame( &P1 );
            next_ani_tick = SDL_GetTicks() + animation_period;

            Fighter_tick_frame( &P2 );
            next_ani_tick = SDL_GetTicks() + animation_period;
        }

        fighters_hurt( &P1, &P2 );
        fighters_hurt( &P2, &P1 );

        display_Fighter( R, &P1 ); //display_Fighter_boxes( R, &P1 );
        display_Fighter( R, &P2 ); //display_Fighter_boxes( R, &P2 );

        SDL_SetRenderDrawColor( R, 0,0,0,255 );
        SDL_RenderLine( R, 0, FLOOR_Y, width, FLOOR_Y );

        SDL_RenderTexture( R, Fundo1, &src_rect, NULL );
        //int flip2;
        //if( P2.direcao > 0 ) flip2 = 1;
        //else if( P2.direcao < 0 ) flip2 = 0;
        //SDL_RenderTextureRotated(R, Melon, NULL, &(P2.hitbox), 0, NULL, flip2);

        
        SDL_RenderPresent(R);
        SDL_framerateDelay( frame_period );

    }//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> / L O O P <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

