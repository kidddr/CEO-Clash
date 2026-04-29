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

//map starts at 0
float MAPw = 0;
float FLOOR_Y = 0;

Transform T;

// STATES
#define NUM_STATES 17
enum{ IDLE, WALK, JUMP, FALL, LLAUNCH, CROUCH, ATK11, ATK12, ATK13, ATK14, BEATEN, DASH, HLAUNCH, LAND, GROUND, GETUP, ADASH };
const char state_names[NUM_STATES][24] = { ">idle", ">walk", ">jump", ">fall", ">llaunch", ">crouch", ">atk11", ">atk12", ">atk13", ">atk14", ">beaten", ">dash", ">hlaunch", ">land", ">ground", ">getup", ">adash" };

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
    float dashspeed;
    int dashtime;
    float jumppower;

    int combo_step;        // golpe atual do combo (0 = fora de combo)
    int combo_cancel_frame;
    int imunidade;

    bool segurar_ground;


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
            //SDL_Log( "std.indices[%d] = %d, sc: %d", S, std.indices[S], state_counts[std.indices[S]] );
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
                            strtrim(buf);
                            SDL_snprintf( pathbuf, 512, "%s/%s", path, buf );
                            F->frames[line] = IMG_LoadTexture( R, pathbuf );
                            if( F->frames[line] == NULL ){
                                SDL_Log( "failed to load frame \"%s\": %s", pathbuf, SDL_GetError() );
                            }
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
    return F->state == JUMP || F->state == FALL || F->state == HLAUNCH;
}

void Fighter_control( Fighter *F, bool cu, bool cd, bool cl, bool cr, bool cA, bool cdash ){

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
        if( F->state != ATK11 && F->state != ATK12 &&
            F->state != ATK13 && F->state != ATK14 ){
            F->state = ATK11; F->frame = 0;
            F->combo_step = 1;
        }
    }

        if( cd && !Fighter_no_ar(F) ){
            F->state = CROUCH; F->frame = 0;
        }
        if( (cr || cl) && cdash ){
            F->vel = v2dzero;          
            F->direcao = (cr - cl);
            F->dashtime = 50;      
            F->state = DASH; F->frame = 0;
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

            if( cd ){
                F->state = CROUCH;
            }
            else{
                F->state = IDLE; F->frame = 0;
            }
            
            break;

        case ATK11:
        case ATK12:
        case ATK13:
            if( cA && F->frame >= F->combo_cancel_frame ){
                if( F->combo_step >= 1 && F->combo_step < 4 ){
                    F->combo_step++;
                    if( F->combo_step == 2 ){
                        F->state = ATK12; F->frame = 0;
                    }
                    else if( F->combo_step == 3 ){
                        desloc.x = F->direcao * 160; // movimento natural de atk, um passo
                        F->state = ATK13; F->frame = 0;
                    }
                    else if( F->combo_step == 4 ){
                        desloc.x = F->direcao * 60;
                        F->state = ATK14; F->frame = 0;
                    }
                }
            }
            break;
        
        case ATK14:
            // Cancel pro dash pra continuar combo de 5 atks
            if( cdash && (cr || cl) && F->frame >= F->combo_cancel_frame ){
                F->vel = v2dzero;
                F->direcao = (cr - cl);
                F->dashtime = 50;
                F->dashspeed = 120; // mais rápido que o HLAUNCH pra acertar o atk5
                F->state = DASH; F->frame = 0;
            }
            break;

        case BEATEN:
            
            break;



       case DASH:
            desloc.x = F->direcao * F->dashspeed;
            F->dashtime -= 1;
        
            if( cA && F->dashtime > 25 ){
                F->vel.x = F->direcao * F->dashspeed;
                F->vel.y = 0;
                F->dashspeed = 37;
                F->state = ADASH; F->frame = 0;
                break;
            }
        
            if( F->dashtime == 0 ){
                F->dashspeed = 37;
                F->state = IDLE; F->frame = 0;
            }
            break;

        

        case ADASH:
            
            break;



        case HLAUNCH:
            F->vel.x *= 0.80; // desacelera suavemente
            if( SDL_fabsf(F->vel.x) < 1.0f ) F->vel.x = 0;
            break;

        case LLAUNCH:
             F->vel.x *= 0.70;
            if( SDL_fabsf(F->vel.x) < 0.5f ) F->vel.x = 0;
            break;


        case GROUND:
            F->segurar_ground = cd;
            break;
        

        case GETUP:
            break;
    }
    








    if( desloc.x != 0 ){// no chao
        F->pos.x += desloc.x;
        F->direcao = (desloc.x > 0)-(desloc.x < 0);
    }

    if( F->vel.x != 0 ){
        F->pos.x += F->vel.x;
    // não atualiza direcao aqui pra manter a cara pro lado que estava
    }

    F->pos.y += F->vel.y;


    if( F->pos.y > FLOOR_Y ){
    F->pos.y = FLOOR_Y;
    F->vel.y = 0;
    F->vel.x = 0;

    if( F->state == HLAUNCH ){
        F->state = LLAUNCH; F->frame = 0;
    } else if( F->state == JUMP || F->state == FALL ){
        F->state = LAND; F->frame = 0;
    }
}


    if( F->pos.x < 0 ){// colisao com a parede Left
        F->pos.x = 0;
    }
    if( F->pos.x > MAPw ){// colisao com a parede Right
        F->pos.x = MAPw;
    }

    /*
    SDL_FRect *hbs = F->hitboxes[ F->state_frame_offsets[ F->state ] + F->frame ];
    int hn = vector_size( hbs );
    for (int h = 0; h < hn; ++h ){
        SDL_FRect *hitbox = hbs + h;

        if( F->pos.x - 0.5 * hitbox->w < 0 ){// colisao com a parede Left
            F->pos.x = 0 + 0.5 * hitbox->w;
        }
        if( F->pos.x + 0.5 * hitbox->w > MAPw ){// colisao com a parede Right
            F->pos.x = MAPw - 0.5 * hitbox->w;
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
        out.x = F->pos.x +  F->anchors[frm].x - box->x - box->w;
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
    //SDL_Log( "get: %g, %g, %g", dst.y, dst.w, dst.h );
    if( F->direcao < 0 ){
        dst.x = F->pos.x +  F->anchors[frm].x - tw;
    } else {
        dst.x = F->pos.x - F->anchors[frm].x;
    }
    return dst;
}



SDL_FRect get_Fighter_boundingbox_now( Fighter *F ){
    int frm = F->state_frame_offsets[ F->state ] + F->frame;
    int hs = vector_size( F->hitboxes[frm] );


    if( hs == 0 || F->hitboxes[frm] == NULL ){
        // Retorna uma box vazia na posição do personagem
        return (SDL_FRect){ F->pos.x, F->pos.y, 0, 0 };
    }

    SDL_FRect box = fighter_box_to_world( F, F->hitboxes[frm]+0 );
    for (int i = 1; i < hs; ++i){
        box = add_rects( &box, F->hitboxes[frm]+i );
    }
    return box;
}




void fighters_hurt( Fighter *attacker, Fighter *defender ){
    bool golpe_forte = ( attacker->state == ATK14 || attacker->state == ADASH );

    if( defender->imunidade == 1 ) return;

    // imunidade 2 é só pro golpe forte passar, mas NÃO fecha a janela ainda
    if( defender->imunidade == 2 && !golpe_forte ) return;

    int Afrm = attacker->state_frame_offsets[ attacker->state ] + attacker->frame;
    int Dfrm = defender->state_frame_offsets[ defender->state ] + defender->frame;
    int hurts = vector_size( attacker->hurtboxes[Afrm] );
    int hits  = vector_size( defender->hitboxes[Dfrm] );

    for (int a = 0; a < hurts; ++a) {
        for( int d = 0; d < hits; ++d ){
            SDL_FRect abox = fighter_box_to_world( attacker, attacker->hurtboxes[Afrm] + a );
            SDL_FRect dbox = fighter_box_to_world( defender, defender->hitboxes[Dfrm] + d );

            if( SDL_FRect_overlap( &abox, &dbox ) ){
                // acertou o ataque!!!
                  defender->direcao = (defender->pos.x < attacker->pos.x) ? 1 : -1; //vira personagem

                if( golpe_forte ){
                    int dir = (attacker->direcao >= 0) ? 1 : -1;
                    bool segundo_lancamento = (defender->imunidade == 2);
                    float forca = segundo_lancamento ? 550.0f : 370.0f;
                    defender->vel.x = dir * forca;
                    defender->vel.y = 0;
                    defender->pos.y = FLOOR_Y;
                    defender->no_controle = false;
                    defender->imunidade = 2; // mantém janela aberta pro próximo HLAUNCH
                    defender->state = HLAUNCH; defender->frame = 0;
                } else {
                    defender->imunidade = 1;
                    defender->state = BEATEN; defender->frame = 0;
                    SDL_Log("Ai, me bateu!!");
                }

                return;
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

    if( F->hitboxes[frm] ){
        for (int i = 0; i < hs; ++i){
            SDL_FRect rct = fighter_box_to_world(F, F->hitboxes[frm] + i);
            rct = apply_transform_frect( &rct, &T ); // PARA A CÂMERA
            SDL_RenderRect( R, &rct );
        }
    }

    if( F->hurtboxes[frm] ){
        hs = vector_size( F->hurtboxes[frm] );
        SDL_SetRenderDrawColor( R, 255, 0, 0, 255 );
        for (int i = 0; i < hs; ++i){
            SDL_FRect rct = fighter_box_to_world(F, F->hurtboxes[frm] + i);
            rct = apply_transform_frect( &rct, &T ); // PARA A CÂMERA
            SDL_RenderRect( R, &rct );
        }
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
                F->frame = 0;
                break;
            case WALK:// Loop animation
                F->frame = 0;
                break;

            case ATK11:
            case ATK12:
            case ATK13:
            case ATK14:
                F->combo_step = 0;
                F->state = IDLE; F->frame = 0;
                break;

            case DASH:
                F->state = IDLE; F->frame = 0;
                break;


            case ADASH:
                F->vel = v2dzero;
                F->combo_step = 0;
                F->state = IDLE; F->frame = 0;
                break;


            case HLAUNCH:
                F->state = LLAUNCH; F->frame = 0;
                break;

            case LLAUNCH:
                F->state = GROUND; F->frame = 0;
                F->imunidade = 1; // fecha possiblidade de combo para o GROUND ser imune
                break;


            
            case GROUND:
                if( F->segurar_ground ){
                    F->frame -= 1;
                } else {
                    F->state = GETUP; F->frame = 0;
                }
                break;
            
            case GETUP:
                F->state = IDLE; F->frame = 0;
                F->imunidade = 0; // vulneravel de novo
                break;
            
            case BEATEN:
                F->state = IDLE; F->frame = 0;
                F->no_controle = false;
                F->imunidade = 0;
                break;
            
            case LAND:
                F->state = IDLE; F->frame = 0;
                F->no_controle = false;
                F->imunidade = 0;
                break;



            default:
                F->frame -= 1; // pra ficar no ultimo frame por padrão
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

    SDL_Texture *Fundo0 = IMG_LoadTexture(R,"Assets/add/add0.png");
    SDL_Texture *Fundo1 = IMG_LoadTexture(R,"Assets/add/add1.png");
    SDL_Texture *Fundo2 = IMG_LoadTexture(R,"Assets/add/add2.png");
    SDL_Texture *Fundo3 = IMG_LoadTexture(R,"Assets/add/add3.png");
    SDL_Texture *Fundo4 = IMG_LoadTexture(R,"Assets/add/add4.png");
    SDL_Texture *Fundo5 = IMG_LoadTexture(R,"Assets/add/add5.png");
    float fundo0w, fundo0h;
    SDL_GetTextureSize(Fundo0, &fundo0w, &fundo0h);
    SDL_GetTextureSize(Fundo1, &fundo0w, &fundo0h);
    SDL_GetTextureSize(Fundo2, &fundo0w, &fundo0h);
    SDL_GetTextureSize(Fundo3, &fundo0w, &fundo0h);
    SDL_GetTextureSize(Fundo4, &fundo0w, &fundo0h);
    SDL_GetTextureSize(Fundo5, &fundo0w, &fundo0h);


    MAPw = fundo0w;
    FLOOR_Y = height - 100;
    float floor_world_y = fundo0h - 200;
    float minzoomout = width / (1.2 * MAPw);
    SDL_Log("mz %g", minzoomout);


    T = (Transform){0,0,0,0,1,1};
    T.cx = cx;
    T.cy = 0;




    //  HITBOXES PERSONAGENS


    Fighter P1 = {0};
    Load_fighter( R, &P1, "Assets/Abilli" );
    P1.pos = v2d( 100, FLOOR_Y );
    P1.walkspeed = 9;
    P1.dashspeed = 50;
    P1.jumppower = -46;
    P1.combo_cancel_frame = 5;
    P1.direcao = 0;

    Fighter P2 = {0};


    Load_fighter( R, &P2, "Assets/Abilli" );
    P2.pos = v2d( width-100, FLOOR_Y );
    P2.walkspeed = 9;
    P2.dashspeed = 50;
    P2.jumppower = -46;
    P2.combo_cancel_frame = 5;
    P2.direcao = 1;


    bool p1u = 0, p1d = 0, p1l = 0, p1r = 0, p1_A = 0, p1_dash = 0; // up down left right
    bool p2u = 0, p2d = 0, p2l = 0, p2r = 0, p2_A = 0, p2_dash = 0;

    
    int frame_period = SDL_lround( 1000 / 60.0 );
    int animation_period = SDL_lround( 1000 / 15.0 );
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
                    else if( event.key.key == 'q') p1_dash = 1;

                    else if( event.key.key == SDLK_UP    ) p2u = 1;
                    else if( event.key.key == SDLK_DOWN  ) p2d = 1;
                    else if( event.key.key == SDLK_LEFT  ) p2l = 1;
                    else if( event.key.key == SDLK_RIGHT ) p2r = 1;
                    else if( event.key.key == 'm' ) p2_A = 1;
                    else if( event.key.key == 'n') p2_dash = 1;
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
                    else if( event.key.key == 'q') p1_dash = 0;

                    else if( event.key.key == SDLK_UP    ) p2u = 0;
                    else if( event.key.key == SDLK_DOWN  ) p2d = 0;
                    else if( event.key.key == SDLK_LEFT  ) p2l = 0;
                    else if( event.key.key == SDLK_RIGHT ) p2r = 0;
                    else if( event.key.key == 'm' ) p2_A = 0;
                    else if( event.key.key == 'n') p2_dash = 0;
                    break;
            }
        }

        SDL_SetRenderDrawColor( R, 200,200,200,255 );
        SDL_RenderClear(R);





///////////////////CAMERA

        float fighters_distx = SDL_fabsf( P1.pos.x - P2.pos.x );
        float fighters_disty = SDL_fabsf( P1.pos.y - P2.pos.y );

        set_scale( &T, T.s );
        float camx = ((P1.pos.x + P2.pos.x) / 2.0f);
        T.tx = camx;

        float visible_height = height * T.invs;
        float camy = floor_world_y - FLOOR_Y * T.invs;
        if (camy < 0) camy = 0;
        if (camy > fundo0h - visible_height) camy = fundo0h - visible_height;
        T.ty = FLOOR_Y - (FLOOR_Y * T.invs);
        set_scale ( &T, constrainD(width / (1.2 * fighters_distx), minzoomout, 1) );

        
        //SDL_Log(">>  %lg, %lg, %g", P1.pos.x, P2.pos.x, camx );

////////////////////




        SDL_FRect src_rect = {camx, camy, width * T.invs, height * T.invs};
        SDL_RenderTexture( R, Fundo0, &src_rect, NULL );
        SDL_RenderTexture( R, Fundo1, &src_rect, NULL );
        SDL_RenderTexture( R, Fundo2, &src_rect, NULL );
        SDL_RenderTexture( R, Fundo3, &src_rect, NULL );
        SDL_RenderTexture( R, Fundo4, &src_rect, NULL );

        Fighter_control( &P1, p1u, p1d, p1l, p1r, p1_A, p1_dash );

        Fighter_control( &P2, p2u, p2d, p2l, p2r, p2_A, p2_dash );
        


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

        display_Fighter( R, &P1 ); display_Fighter_boxes( R, &P1 );
        display_Fighter( R, &P2 ); display_Fighter_boxes( R, &P2 );

        SDL_SetRenderDrawColor( R, 0,0,0,255 );
        SDL_RenderLine( R, 0, FLOOR_Y, width, FLOOR_Y );

        SDL_RenderTexture( R, Fundo5, &src_rect, NULL );

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

