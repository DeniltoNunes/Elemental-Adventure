#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>


// ============================================================
// CONFIGURAÇÕES
// ============================================================

#define NUM_SLIMES 2          // se criar o 3º slime, mude para 3 e inicialize ele

// Escala dos objetos do cenário (aumente/diminua à vontade)
#define ESCALA_ARVORE      2.0f
#define ESCALA_ARVORE_PEQ  2.0f
#define ESCALA_CASA        2.0f

// Hitbox do slime dentro do frame 64x64 (ajuste olhando o modo debug)
#define SLIME_HIT_X 16
#define SLIME_HIT_Y 20
#define SLIME_HIT_W 32
#define SLIME_HIT_H 28

// Hitbox do jogador dentro do sprite 96x96 (32 * escala 3)
#define PLAYER_HIT_X 30
#define PLAYER_HIT_Y 14
#define PLAYER_HIT_W 36
#define PLAYER_HIT_H 62

// 1 = desenha as hitboxes na tela para você ajustar os valores
#define DEBUG_HITBOX 0


// ============================================================
// SLIME
// ============================================================

typedef struct {

    float x;
    float y;

    float velocidade;
    int direcao;

    int frame;
    int contador;

    int limite_esquerda;
    int limite_direita;

    // Vida
    int vida;

    // Tempo que fica vermelho
    int dano_timer;

} Slime;


// ============================================================
// FUNÇÃO: COLISÃO RETANGULAR
// ============================================================

int colisao(
    float x1, float y1, float largura1, float altura1,
    float x2, float y2, float largura2, float altura2
) {

    if (x1 < x2 + largura2 &&
        x1 + largura1 > x2 &&
        y1 < y2 + altura2 &&
        y1 + altura1 > y2) {

        return 1;
    }

    return 0;
}


// ============================================================
// FUNÇÃO: COLISÃO JOGADOR x SLIME (usa as hitboxes ajustadas)
// ============================================================

int colisao_player_slime(float px, float py, Slime* s) {

    return colisao(
        px + PLAYER_HIT_X, py + PLAYER_HIT_Y, PLAYER_HIT_W, PLAYER_HIT_H,
        s->x + SLIME_HIT_X, s->y + SLIME_HIT_Y, SLIME_HIT_W, SLIME_HIT_H
    );
}


// ============================================================
// MAIN
// ============================================================

int main() {

    ALLEGRO_DISPLAY* janela = NULL;
    ALLEGRO_EVENT_QUEUE* fila = NULL;
    ALLEGRO_TIMER* timer = NULL;

    ALLEGRO_BITMAP* personagem = NULL;
    ALLEGRO_BITMAP* grama = NULL;
    ALLEGRO_BITMAP* caminho = NULL;
    ALLEGRO_BITMAP* agua = NULL;

    ALLEGRO_BITMAP* slime_sprite = NULL;

    ALLEGRO_BITMAP* arvore = NULL;
    ALLEGRO_BITMAP* arvore_pequena = NULL;
    ALLEGRO_BITMAP* casa = NULL;
    ALLEGRO_BITMAP* bau = NULL;

    ALLEGRO_EVENT evento;


    // ========================================================
    // TAMANHO DA JANELA
    // ========================================================

    int xj = 1280;
    int yj = 720;


    // ========================================================
    // JOGADOR
    // ========================================================

    int x = 600;
    int y = 450;

    int sair = 0;


    // ========================================================
    // TECLAS
    // ========================================================

    int cima = 0;
    int baixo = 0;
    int esquerda = 0;
    int direita = 0;


    // ========================================================
    // SPRITE DO JOGADOR
    // ========================================================

    int largura_sprite = 32;
    int altura_sprite = 32;

    int escala = 3;

    int largura_personagem = largura_sprite * escala;
    int altura_personagem = altura_sprite * escala;


    // ========================================================
    // ANIMAÇÃO
    // ========================================================

    int coluna = 0;
    int linha = 0;

    int contador_animacao = 0;


    // ========================================================
    // ATAQUE
    // ========================================================

    int atacando = 0;

    int frame_ataque = 0;

    int contador_ataque = 0;

    /*
        0 = baixo
        1 = direita
        2 = cima
        3 = esquerda
    */

    int ultima_direcao = 0;
    int ataque_acertou = 0;


    // ========================================================
    // INICIALIZAÇÃO
    // ========================================================

    if (!al_init()) {

        printf("Erro ao iniciar o Allegro\n");

        return -1;
    }

    if (!al_install_keyboard()) {

        printf("Erro ao iniciar o teclado\n");

        return -1;
    }

    al_init_primitives_addon();
    al_init_image_addon();

    // ========================================================
    // JANELA
    // ========================================================

    janela = al_create_display(xj, yj);

    if (!janela) {
        printf("Erro ao criar a janela.\\n");
        return -1;
    }

    // ========================================================
    // PLAYER
    // ========================================================

    personagem = al_load_bitmap(
        "Assets/player.png"
    );

    // ========================================================
    // TILES
    // ========================================================

    grama = al_load_bitmap(
        "Assets/Grass_Middle.png"
    );

    caminho = al_load_bitmap(
        "Assets/Path_Middle.png"
    );

    agua = al_load_bitmap(
        "Assets/Water_Middle.png"
    );

    // ========================================================
    // SLIME
    // ========================================================

    slime_sprite = al_load_bitmap(
        "Assets/Slime_Green.png"
    );

    // ========================================================
    // DECORAÇÃO
    // ========================================================

    arvore = al_load_bitmap(
        "Assets/Oak_Tree.png"
    );

    arvore_pequena = al_load_bitmap(
        "Assets/Oak_Tree_Small.png"
    );

    casa = al_load_bitmap(
        "Assets/House_1_Wood_Base_Blue.png"
    );

    bau = al_load_bitmap(
        "Assets/Chest.png"
    );

    // Verificar se todos os arquivos foram carregados
    if (!personagem || !grama || !caminho || !agua ||
        !slime_sprite || !arvore || !arvore_pequena || !casa || !bau) {

        printf("ERRO: nao foi possivel carregar um ou mais arquivos da pasta Assets.\n");
        printf("player: %s\n", personagem ? "OK" : "FALHOU");
        printf("grama: %s\n", grama ? "OK" : "FALHOU");
        printf("caminho: %s\n", caminho ? "OK" : "FALHOU");
        printf("agua: %s\n", agua ? "OK" : "FALHOU");
        printf("slime: %s\n", slime_sprite ? "OK" : "FALHOU");
        printf("arvore: %s\n", arvore ? "OK" : "FALHOU");
        printf("arvore pequena: %s\n", arvore_pequena ? "OK" : "FALHOU");
        printf("casa: %s\n", casa ? "OK" : "FALHOU");
        printf("bau: %s\n", bau ? "OK" : "FALHOU");
        return -1;
    }

    // ========================================================
    // TIMER
    // ========================================================

    timer = al_create_timer(
        1.0 / 60.0
    );

    // ========================================================
    // FILA
    // ========================================================

    fila = al_create_event_queue();

    al_register_event_source(fila, al_get_display_event_source(janela));

    al_register_event_source(fila, al_get_keyboard_event_source());

    al_register_event_source(
        fila,
        al_get_timer_event_source(timer)
    );


    al_start_timer(timer);


    // ========================================================
    // SLIMES
    // ========================================================

    Slime slimes[NUM_SLIMES];


    // SLIME 1

    slimes[0].x = 300;
    slimes[0].y = 300;

    slimes[0].velocidade = 1.0;
    slimes[0].direcao = 1;

    slimes[0].frame = 0;
    slimes[0].contador = 0;

    slimes[0].limite_esquerda = 200;
    slimes[0].limite_direita = 450;

    slimes[0].vida = 2;
    slimes[0].dano_timer = 0;


    // SLIME 2

    slimes[1].x = 850;
    slimes[1].y = 500;

    slimes[1].velocidade = 1.5;
    slimes[1].direcao = -1;

    slimes[1].frame = 0;
    slimes[1].contador = 15;

    slimes[1].limite_esquerda = 700;
    slimes[1].limite_direita = 1050;

    slimes[1].vida = 2;
    slimes[1].dano_timer = 0;


    // ========================================================
    // LOOP PRINCIPAL
    // ========================================================

    while (!sair) {

        al_wait_for_event(
            fila,
            &evento
        );


        // ====================================================
        // FECHAR
        // ====================================================

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {

            sair = 1;
        }


        // ====================================================
        // TECLA PRESSIONADA
        // ====================================================

        if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {

            if (evento.keyboard.keycode == ALLEGRO_KEY_UP)
                cima = 1;

            if (evento.keyboard.keycode == ALLEGRO_KEY_DOWN)
                baixo = 1;

            if (evento.keyboard.keycode == ALLEGRO_KEY_LEFT)
                esquerda = 1;

            if (evento.keyboard.keycode == ALLEGRO_KEY_RIGHT)
                direita = 1;


            // =================================================
            // ATAQUE
            // =================================================

            if (evento.keyboard.keycode == ALLEGRO_KEY_E) {

                if (!atacando) {

                    atacando = 1;
                    frame_ataque = 0;
                    contador_ataque = 0;
                    ataque_acertou = 0;
                }
            }

            if (evento.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                sair = 1;
        }


        // ====================================================
        // TECLA SOLTA
        // ====================================================

        if (evento.type == ALLEGRO_EVENT_KEY_UP) {

            if (evento.keyboard.keycode == ALLEGRO_KEY_UP)
                cima = 0;

            if (evento.keyboard.keycode == ALLEGRO_KEY_DOWN)
                baixo = 0;

            if (evento.keyboard.keycode == ALLEGRO_KEY_LEFT)
                esquerda = 0;

            if (evento.keyboard.keycode == ALLEGRO_KEY_RIGHT)
                direita = 0;
        }


        // ====================================================
        // TIMER
        // ====================================================

        if (evento.type == ALLEGRO_EVENT_TIMER) {

            // =================================================
            // MOVIMENTO DO JOGADOR
            // =================================================

            int velocidade = 5;

            int novo_x = x;
            int novo_y = y;

            if (!atacando) {

                if (cima) {
                    novo_y -= velocidade;
                    linha = 5;
                    ultima_direcao = 2;
                }

                else if (baixo) {
                    novo_y += velocidade;
                    linha = 3;
                    ultima_direcao = 0;
                }

                else if (direita) {
                    novo_x += velocidade;
                    linha = 4;
                    ultima_direcao = 1;
                }

                else if (esquerda) {
                    novo_x -= velocidade;
                    linha = 4;
                    ultima_direcao = 3;
                }
            }


            // =================================================
            // LIMITES DA TELA
            // =================================================

            if (novo_x < 0)
                novo_x = 0;

            if (novo_x > xj - largura_personagem)
                novo_x = xj - largura_personagem;

            if (novo_y < 0)
                novo_y = 0;

            if (novo_y > yj - altura_personagem)
                novo_y = yj - altura_personagem;


            // =================================================
            // COLISÃO DO JOGADOR COM SLIMES
            // (eixos separados: o jogador desliza em vez de travar)
            // =================================================

            int pode_mover_x = 1;
            int pode_mover_y = 1;

            if (!atacando) {

                for (int i = 0; i < NUM_SLIMES; i++) {

                    Slime* s = &slimes[i];

                    if (s->vida <= 0)
                        continue;

                    if (colisao_player_slime(novo_x, y, s))
                        pode_mover_x = 0;

                    if (colisao_player_slime(x, novo_y, s))
                        pode_mover_y = 0;
                }
            }

            if (pode_mover_x)
                x = novo_x;

            if (pode_mover_y)
                y = novo_y;


            // =================================================
            // ANIMAÇÃO DO JOGADOR
            // =================================================

            if (!atacando) {

                if (cima || baixo || esquerda || direita) {

                    contador_animacao++;

                    if (contador_animacao >= 8) {

                        coluna++;

                        if (coluna >= 6)
                            coluna = 0;

                        contador_animacao = 0;
                    }
                }
            }


            // =================================================
            // ATAQUE
            // =================================================

            if (atacando) {

                contador_ataque++;


                // ---------------------------------------------
                // MOMENTO DO IMPACTO
                // ---------------------------------------------

                if (contador_ataque == 4 &&
                    !ataque_acertou) {

                    /*
                        Área de ataque da espada.
                    */

                    float ataque_x = x;
                    float ataque_y = y;

                    float ataque_largura = largura_personagem;
                    float ataque_altura = altura_personagem;


                    // DIREITA

                    if (ultima_direcao == 1) {

                        ataque_x += largura_personagem - 10;

                        ataque_largura = 55;
                    }


                    // ESQUERDA

                    else if (ultima_direcao == 3) {

                        ataque_x -= 45;

                        ataque_largura = 55;
                    }


                    // CIMA

                    else if (ultima_direcao == 2) {

                        ataque_y -= 45;

                        ataque_altura = 55;
                    }


                    // BAIXO

                    else if (ultima_direcao == 0) {

                        ataque_y += altura_personagem - 10;

                        ataque_altura = 55;
                    }


                    // -----------------------------------------
                    // TESTAR SLIMES
                    // -----------------------------------------

                    for (int i = 0; i < NUM_SLIMES; i++) {

                        Slime* s = &slimes[i];

                        if (s->vida <= 0)
                            continue;

                        if (colisao(
                            ataque_x,
                            ataque_y,
                            ataque_largura,
                            ataque_altura,

                            s->x + SLIME_HIT_X,
                            s->y + SLIME_HIT_Y,
                            SLIME_HIT_W,
                            SLIME_HIT_H
                        )) {

                            // DANO
                            s->vida--;

                            // EFEITO VERMELHO
                            s->dano_timer = 12;

                            ataque_acertou = 1;

                            printf(
                                "Slime atingido! Vida: %d\n",
                                s->vida
                            );

                            break;
                        }
                    }
                }


                // ---------------------------------------------
                // TERMINAR ATAQUE
                // ---------------------------------------------

                if (contador_ataque >= 8) {

                    frame_ataque++;

                    contador_ataque = 0;

                    if (frame_ataque >= 2) {

                        atacando = 0;
                        frame_ataque = 0;
                        contador_ataque = 0;
                        coluna = 0;
                    }
                }
            }


            // =================================================
            // SLIMES
            // =================================================

            for (int i = 0; i < NUM_SLIMES; i++) {

                Slime* s = &slimes[i];

                // Morto
                if (s->vida <= 0)
                    continue;


                // ---------------------------------------------
                // MOVIMENTO (não atravessa o jogador)
                // ---------------------------------------------

                float prox_x = s->x + s->velocidade * s->direcao;

                Slime teste = *s;
                teste.x = prox_x;

                if (colisao_player_slime(x, y, &teste)) {

                    // bateu no jogador, volta
                    s->direcao *= -1;
                }

                else {

                    s->x = prox_x;
                }


                if (s->x <= s->limite_esquerda) {

                    s->x = s->limite_esquerda;
                    s->direcao = 1;
                }

                if (s->x >= s->limite_direita) {

                    s->x = s->limite_direita;
                    s->direcao = -1;
                }


                // ---------------------------------------------
                // ANIMAÇÃO
                // ---------------------------------------------

                s->contador++;

                if (s->contador >= 10) {

                    s->frame++;

                    s->contador = 0;

                    if (s->frame >= 4)
                        s->frame = 0;
                }


                // ---------------------------------------------
                // DIMINUIR TEMPO DO VERMELHO
                // ---------------------------------------------

                if (s->dano_timer > 0)
                    s->dano_timer--;
            }


            // =================================================
            // LIMPAR A TELA
            // =================================================

            al_clear_to_color(al_map_rgb(0, 0, 0));

            // =================================================
            // GRAMA
            // =================================================

            for (int yy = 0; yy < yj; yy += 16) {
                for (int xx = 0; xx < xj; xx += 16) {
                    al_draw_bitmap(grama, xx, yy, 0);
                }
            }


            // =================================================
            // CAMINHO
            // =================================================

            for (int xx = 0; xx < xj; xx += 16) {

                al_draw_bitmap(caminho, xx, 360, 0);

                al_draw_bitmap(caminho, xx, 376, 0);
            }


            // =================================================
            // LAGO
            // =================================================

            for (int yy = 100; yy < 260; yy += 16) {

                for (int xx = 650; xx < 850; xx += 16) {
                    al_draw_bitmap(agua, xx, yy, 0);
                }
            }


            // =================================================
            // CASA (com escala)
            // =================================================

            al_draw_scaled_bitmap(
                casa,
                0, 0,
                al_get_bitmap_width(casa),
                al_get_bitmap_height(casa),
                40, 60,
                al_get_bitmap_width(casa) * ESCALA_CASA,
                al_get_bitmap_height(casa) * ESCALA_CASA,
                0
            );


            // =================================================
            // ÁRVORES (com escala)
            // =================================================

            int sw = al_get_bitmap_width(arvore);
            int sh = al_get_bitmap_height(arvore);

            float aw = sw * ESCALA_ARVORE;
            float ah = sh * ESCALA_ARVORE;

            al_draw_scaled_bitmap(arvore, 0, 0, sw, sh, 0, 0, aw, ah, 0);
            al_draw_scaled_bitmap(arvore, 0, 0, sw, sh, 1100, 20, aw, ah, 0);
            al_draw_scaled_bitmap(arvore, 0, 0, sw, sh, 1120, 300, aw, ah, 0);
            al_draw_scaled_bitmap(arvore, 0, 0, sw, sh, 40, 300, aw, ah, 0);


            // =================================================
            // ÁRVORES PEQUENAS (com escala)
            // =================================================

            float p = 48 * ESCALA_ARVORE_PEQ;

            al_draw_scaled_bitmap(arvore_pequena, 0, 0, 48, 48, 300, 100, p, p, 0);
            al_draw_scaled_bitmap(arvore_pequena, 0, 0, 48, 48, 500, 150, p, p, 0);
            al_draw_scaled_bitmap(arvore_pequena, 0, 0, 48, 48, 1000, 400, p, p, 0);


            // =================================================
            // BAÚ
            // =================================================

            al_draw_scaled_bitmap(bau, 0, 0, 16, 16, 520, 300, 32, 32, 0);


            // =================================================
            // SLIMES
            // =================================================

            for (int i = 0; i < NUM_SLIMES; i++) {

                Slime* s = &slimes[i];

                if (s->vida <= 0)
                    continue;

                // SLIME NORMAL
                if (s->dano_timer <= 0) {

                    al_draw_bitmap_region(
                        slime_sprite,
                        s->frame * 64, 0, 64, 64,
                        s->x, s->y, 0
                    );
                }

                // SLIME VERMELHO
                else {

                    al_draw_tinted_bitmap_region(
                        slime_sprite,
                        al_map_rgba(255, 50, 50, 255),
                        s->frame * 64, 0, 64, 64,
                        s->x, s->y, 0
                    );
                }
            }


            // =================================================
            // ATAQUE (desenho)
            // =================================================

            if (atacando) {

                // DIREITA
                if (ultima_direcao == 1) {

                    al_draw_scaled_bitmap(
                        personagem,
                        frame_ataque * largura_sprite, 7 * altura_sprite,
                        largura_sprite, altura_sprite,
                        x, y, largura_personagem, altura_personagem, 0
                    );
                }

                // ESQUERDA
                else if (ultima_direcao == 3) {

                    al_draw_scaled_bitmap(
                        personagem,
                        frame_ataque * largura_sprite, 7 * altura_sprite,
                        largura_sprite, altura_sprite,
                        x, y, largura_personagem, altura_personagem,
                        ALLEGRO_FLIP_HORIZONTAL
                    );
                }

                // CIMA
                else if (ultima_direcao == 2) {

                    al_draw_scaled_bitmap(
                        personagem,
                        frame_ataque * largura_sprite, 8 * altura_sprite,
                        largura_sprite, altura_sprite,
                        x, y, largura_personagem, altura_personagem, 0
                    );
                }

                // BAIXO
                else {

                    al_draw_scaled_bitmap(
                        personagem,
                        frame_ataque * largura_sprite, 6 * altura_sprite,
                        largura_sprite, altura_sprite,
                        x, y, largura_personagem, altura_personagem, 0
                    );
                }
            }


            // =================================================
            // JOGADOR
            // =================================================

            else {

                if (ultima_direcao == 3) {

                    al_draw_scaled_bitmap(
                        personagem,
                        coluna * largura_sprite, linha * altura_sprite,
                        largura_sprite, altura_sprite,
                        x, y, largura_personagem, altura_personagem,
                        ALLEGRO_FLIP_HORIZONTAL
                    );
                }

                else {

                    al_draw_scaled_bitmap(
                        personagem,
                        coluna * largura_sprite, linha * altura_sprite,
                        largura_sprite, altura_sprite,
                        x, y, largura_personagem, altura_personagem, 0
                    );
                }
            }

            // Mostrar o quadro na tela
            al_flip_display();

        } // fim do bloco do timer
        } // fim do while principal

        // ========================================================
        // FINALIZAR
        // ========================================================

        al_destroy_bitmap(personagem);

            al_destroy_bitmap(grama);
            al_destroy_bitmap(caminho);
            al_destroy_bitmap(agua);

            al_destroy_bitmap(slime_sprite);

            al_destroy_bitmap(arvore);
            al_destroy_bitmap(arvore_pequena);
            al_destroy_bitmap(casa);
            al_destroy_bitmap(bau);

            al_destroy_timer(timer);
            al_destroy_event_queue(fila);
        al_destroy_display(janela);

        return 0;
}
