#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>


// ============================================================
// CONFIGURAÇÕES E CONSTANTES
// ============================================================

#define LARGURA_TELA 1280
#define ALTURA_TELA  720

#define NUM_SLIMES   2
#define NUM_ARVORES  4
#define NUM_ARVORES_PEQ 3

// Escalas do cenário
#define ESCALA_ARVORE       2.0f
#define ESCALA_ARVORE_PEQ   2.0f
#define ESCALA_CASA         3.0f

// Atributos de Vida
#define VIDA_MAXIMA_PLAYER 5
#define VIDA_MAXIMA_SLIME  2
#define TEMPO_INVULNERAVEL 60 // 1 segundo (60 frames)

// Configurações do Inventário HUD
#define NUM_SLOTS    9
#define TAMANHO_SLOT 50
#define ESPACO_SLOT  8

// Configurações das Barras de Vida Flutuantes
#define LARGURA_BARRA_PLAYER 60.0f
#define ALTURA_BARRA_PLAYER  8.0f
#define LARGURA_BARRA_SLIME  40.0f
#define ALTURA_BARRA_SLIME   6.0f


// ============================================================
// ESTRUTURAS DE DADOS
// ============================================================

typedef struct {
    float x, y;
    int largura, altura;
    int escala;
    int largura_scaled, altura_scaled;

    int vida;
    int invulneravel_timer;

    int velocidade;
    int cima, baixo, esquerda, direita;

    int coluna, linha;
    int contador_animacao;

    int atacando;
    int frame_ataque;
    int contador_ataque;
    int ultima_direcao; // 0 = baixo, 1 = direita, 2 = cima, 3 = esquerda
    int ataque_acertou;

    // Hitbox
    int hit_x, hit_y, hit_w, hit_h;
} Player;

typedef struct {
    float x, y;
    float velocidade;
    int direcao;

    int frame;
    int contador;

    int limite_esquerda;
    int limite_direita;

    int vida;
    int dano_timer;

    // Hitbox
    int hit_x, hit_y, hit_w, hit_h;
} Slime;

typedef struct {
    float x, y;
} Posicao2D;

typedef struct {
    float x, y, w, h;
} Retangulo;

typedef enum { CENA_EXTERIOR, CENA_INTERIOR } Cena;


// ============================================================
// PROTÓTIPOS DAS FUNÇÕES
// ============================================================

int colisao(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);
int colisao_player_slime(Player* p, Slime* s, float novo_x, float novo_y);
int colisao_player_rect(Player* p, Retangulo r, float novo_x, float novo_y);

void init_player(Player* p);
void init_slimes(Slime slimes[]);

void atualizar_player(Player* p, Slime slimes[], int n_slimes,
    Retangulo obst[], int n_obst, Retangulo area);

//void atualizar_slimes(Slime slimes[], Player* p);
void atualizar_slimes(Slime slimes[], Player* p, Retangulo obst[], int n_obst);
void atualizar_slimes(Slime slimes[], Player* p, Retangulo obst[], int n_obst) {
    for (int i = 0; i < NUM_SLIMES; i++) {
        if (slimes[i].vida <= 0) continue;

        float prox_x = slimes[i].x + slimes[i].velocidade * slimes[i].direcao;
        Slime teste = slimes[i];
        teste.x = prox_x;

        // Bateu no jogador?
        int bateu = colisao_player_slime(p, &teste, p->x, p->y);

        // Bateu em algum obstáculo (casa, etc.)?
        for (int j = 0; j < n_obst && !bateu; j++) {
            bateu = colisao(
                teste.x + teste.hit_x, teste.y + teste.hit_y, teste.hit_w, teste.hit_h,
                obst[j].x, obst[j].y, obst[j].w, obst[j].h
            );
        }

        if (bateu) {
            slimes[i].direcao *= -1;
        }
        else {
            slimes[i].x = prox_x;
        }

        if (slimes[i].x <= slimes[i].limite_esquerda) {
            slimes[i].x = slimes[i].limite_esquerda;
            slimes[i].direcao = 1;
        }
        if (slimes[i].x >= slimes[i].limite_direita) {
            slimes[i].x = slimes[i].limite_direita;
            slimes[i].direcao = -1;
        }

        slimes[i].contador++;
        if (slimes[i].contador >= 10) {
            slimes[i].frame = (slimes[i].frame + 1) % 4;
            slimes[i].contador = 0;
        }

        if (slimes[i].dano_timer > 0) slimes[i].dano_timer--;
    }
}

void desenhar_barra_vida(float x, float y, float largura, float altura, int vida_atual, int vida_maxima);
void desenhar_cenario(ALLEGRO_BITMAP* grama, ALLEGRO_BITMAP* caminho, ALLEGRO_BITMAP* agua,
    ALLEGRO_BITMAP* casa, ALLEGRO_BITMAP* arvore, ALLEGRO_BITMAP* arvore_peq,
    ALLEGRO_BITMAP* bau, Posicao2D arvores[], Posicao2D arvores_peq[]);
void desenhar_interior(void);
void desenhar_hud(int slot_selecionado);


// ============================================================
// FUNÇÃO PRINCIPAL (MAIN)
// ============================================================

int main() {

    ALLEGRO_DISPLAY* janela = NULL;
    ALLEGRO_EVENT_QUEUE* fila = NULL;
    ALLEGRO_TIMER* timer = NULL;

    // Bitmaps
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
    int sair = 0;
    int slot_selecionado = 0;

    // Posições estáticas do cenário
    Posicao2D arvores[NUM_ARVORES] = { { 0, 0 },{ 1100, 20 },{ 1120, 300 },{ 40, 300 } };
    Posicao2D arvores_pequenas[NUM_ARVORES_PEQ] = { { 300, 100 },{ 500, 150 },{ 1000, 400 } };

    // Entidades
    Player player;
    Slime slimes[NUM_SLIMES];

    // Cena atual e dados da casa
    Cena cena = CENA_EXTERIOR;
    float casa_x = 40, casa_y = 60, casa_w = 0, casa_h = 0;

    Retangulo tela_ext = { 0, 0, LARGURA_TELA, ALTURA_TELA };
    Retangulo sala = { 240, 120, 800, 480 };
    Retangulo parede_casa, porta_ext;
    Retangulo porta_int = { 600, 570, 80, 30 };

    // ========================================================
    // INICIALIZAÇÃO DA ALLEGRO
    // ========================================================

    if (!al_init() || !al_install_keyboard()) {
        printf("Erro ao inicializar o Allegro ou o teclado.\n");
        return -1;
    }

    al_init_primitives_addon();
    al_init_image_addon();

    janela = al_create_display(LARGURA_TELA, ALTURA_TELA);
    timer = al_create_timer(1.0 / 60.0);
    fila = al_create_event_queue();

    // Carregamento de Sprites
    personagem = al_load_bitmap("Assets/Player.png");
    grama = al_load_bitmap("Assets/Grass_Middle.png");
    caminho = al_load_bitmap("Assets/Path_Middle.png");
    agua = al_load_bitmap("Assets/Water_Middle.png");
    slime_sprite = al_load_bitmap("Assets/Slime_Green.png");
    arvore = al_load_bitmap("Assets/Oak_Tree.png");
    arvore_pequena = al_load_bitmap("Assets/Oak_Tree_Small.png");
    casa = al_load_bitmap("Assets/House_1_Wood_Base_Blue.png");
    bau = al_load_bitmap("Assets/Chest.png");

    if (!personagem || !grama || !caminho || !agua || !slime_sprite ||
        !arvore || !arvore_pequena || !casa || !bau) {
        printf("ERRO: Nao foi possivel carregar um ou mais arquivos da pasta Assets.\n");
        return -1;
    }

    casa_w = al_get_bitmap_width(casa) * ESCALA_CASA;
    casa_h = al_get_bitmap_height(casa) * ESCALA_CASA;

    // Referência: borda de baixo do retângulo vermelho antigo
    float base_y = casa_y + casa_h - 10;

    // Corpo da casa (só a parte visível, sem as sobras transparentes)
    parede_casa.x = casa_x + 40;      // começa onde a parede/telhado começa
    parede_casa.w = 210;              // largura visível da casa
    parede_casa.y = base_y - 369;      // altura do beiral do telhado
    parede_casa.h = 335;              // vai até o pé da casa

    // Zona da porta (a porta é arqueada e fica à esquerda do centro)
    porta_ext.x = casa_x + 100;
    porta_ext.w = 48;
    porta_ext.y = base_y + -35;        // encosta na base da casa
    porta_ext.h = 10;

    al_register_event_source(fila, al_get_display_event_source(janela));
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_timer_event_source(timer));

    al_start_timer(timer);

    // Inicializar Entidades
    init_player(&player);
    init_slimes(slimes);

    // ========================================================
    // LOOP PRINCIPAL DO JOGO
    // ========================================================

    while (!sair) {

        al_wait_for_event(fila, &evento);

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            sair = 1;
        }

        // --- ENTRADA DO TECLADO ---
        if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {
            switch (evento.keyboard.keycode) {
            case ALLEGRO_KEY_UP:    player.cima = 1; break;
            case ALLEGRO_KEY_DOWN:  player.baixo = 1; break;
            case ALLEGRO_KEY_LEFT:  player.esquerda = 1; break;
            case ALLEGRO_KEY_RIGHT: player.direita = 1; break;
            case ALLEGRO_KEY_E:
                if (!player.atacando) {
                    player.atacando = 1;
                    player.frame_ataque = 0;
                    player.contador_ataque = 0;
                    player.ataque_acertou = 0;
                }
                break;
            case ALLEGRO_KEY_ESCAPE: sair = 1; break;
            }

            if (evento.keyboard.keycode >= ALLEGRO_KEY_1 && evento.keyboard.keycode <= ALLEGRO_KEY_9) {
                slot_selecionado = evento.keyboard.keycode - ALLEGRO_KEY_1;
            }
        }

        if (evento.type == ALLEGRO_EVENT_KEY_UP) {
            switch (evento.keyboard.keycode) {
            case ALLEGRO_KEY_UP:    player.cima = 0; break;
            case ALLEGRO_KEY_DOWN:  player.baixo = 0; break;
            case ALLEGRO_KEY_LEFT:  player.esquerda = 0; break;
            case ALLEGRO_KEY_RIGHT: player.direita = 0; break;
            }
        }

        // --- ATUALIZAÇÃO E LÓGICA (UPDATE) ---
        if (evento.type == ALLEGRO_EVENT_TIMER) {

            if (cena == CENA_EXTERIOR) {
                atualizar_player(&player, slimes, NUM_SLIMES, &parede_casa, 1, tela_ext);
                void atualizar_slimes(Slime slimes[], Player * p, Retangulo obst[], int n_obst);

                // Entrar na casa
                if (colisao_player_rect(&player, porta_ext, player.x, player.y)) {
                    cena = CENA_INTERIOR;
                    player.x = 640 - (player.hit_x + player.hit_w / 2);
                    player.y = 480;
                    player.ultima_direcao = 2;
                }
            }
            else {
                atualizar_player(&player, slimes, 0, NULL, 0, sala);

                // Sair da casa
                if (colisao_player_rect(&player, porta_int, player.x, player.y)) {
                    cena = CENA_EXTERIOR;
                    player.x = casa_x + casa_w / 2 - (player.hit_x + player.hit_w / 2);
                    player.y = porta_ext.y + porta_ext.h + 5 - player.hit_y;
                    player.ultima_direcao = 0;
                }
            }

            // --- RENDERIZAÇÃO (DRAW) ---
            al_clear_to_color(al_map_rgb(0, 0, 0));

            if (cena == CENA_EXTERIOR) {
                // 1. Cenário
                desenhar_cenario(grama, caminho, agua, casa, arvore, arvore_pequena, bau, arvores, arvores_pequenas);

                // 2. Slimes
                for (int i = 0; i < NUM_SLIMES; i++) {
                    if (slimes[i].vida <= 0) continue;

                    if (slimes[i].dano_timer <= 0) {
                        al_draw_bitmap_region(slime_sprite, slimes[i].frame * 64, 0, 64, 64, slimes[i].x, slimes[i].y, 0);
                    }
                    else {
                        al_draw_tinted_bitmap_region(
                            slime_sprite, al_map_rgba(255, 50, 50, 255),
                            slimes[i].frame * 64, 0, 64, 64, slimes[i].x, slimes[i].y, 0
                        );
                    }

                    // Barra de vida flutuante do Slime
                    desenhar_barra_vida(
                        slimes[i].x + (64 - LARGURA_BARRA_SLIME) / 2.0f,
                        slimes[i].y + 10,
                        LARGURA_BARRA_SLIME, ALTURA_BARRA_SLIME,
                        slimes[i].vida, VIDA_MAXIMA_SLIME
                    );
                }

                // DEBUG: descomente para ver as hitboxes da casa
                // (vermelho = parede, verde = porta)
                
                al_draw_rectangle(parede_casa.x, parede_casa.y, parede_casa.x + parede_casa.w, parede_casa.y + parede_casa.h, al_map_rgb(255, 0, 0), 2);
                al_draw_rectangle(porta_ext.x, porta_ext.y, porta_ext.x + porta_ext.w, porta_ext.y + porta_ext.h, al_map_rgb(0, 255, 0), 2);
                
            }
            else {
                desenhar_interior();
            }

            // 3. Jogador
            if (player.invulneravel_timer % 10 < 5) {
                int flip = (player.ultima_direcao == 3) ? ALLEGRO_FLIP_HORIZONTAL : 0;
                int linha_sprite = player.linha;

                if (player.atacando) {
                    if (player.ultima_direcao == 1 || player.ultima_direcao == 3) linha_sprite = 7;
                    else if (player.ultima_direcao == 2) linha_sprite = 8;
                    else linha_sprite = 6;

                    al_draw_scaled_bitmap(
                        personagem,
                        player.frame_ataque * player.largura, linha_sprite * player.altura,
                        player.largura, player.altura,
                        player.x, player.y, player.largura_scaled, player.altura_scaled, flip
                    );
                }
                else {
                    al_draw_scaled_bitmap(
                        personagem,
                        player.coluna * player.largura, player.linha * player.altura,
                        player.largura, player.altura,
                        player.x, player.y, player.largura_scaled, player.altura_scaled, flip
                    );
                }
            }

            // Barra de vida flutuante do Jogador
            if (player.vida > 0) {
                desenhar_barra_vida(
                    player.x + (player.largura_scaled - LARGURA_BARRA_PLAYER) / 2.0f,
                    player.y - 12,
                    LARGURA_BARRA_PLAYER, ALTURA_BARRA_PLAYER,
                    player.vida, VIDA_MAXIMA_PLAYER
                );
            }

            // 4. Interface (HUD)
            desenhar_hud(slot_selecionado);

            al_flip_display();
        }
    }

    // ========================================================
    // FINALIZAÇÃO DE RECURSOS
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


// ============================================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ============================================================

int colisao(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

int colisao_player_slime(Player* p, Slime* s, float novo_x, float novo_y) {
    return colisao(
        novo_x + p->hit_x, novo_y + p->hit_y, p->hit_w, p->hit_h,
        s->x + s->hit_x, s->y + s->hit_y, s->hit_w, s->hit_h
    );
}

int colisao_player_rect(Player* p, Retangulo r, float novo_x, float novo_y) {
    return colisao(
        novo_x + p->hit_x, novo_y + p->hit_y, p->hit_w, p->hit_h,
        r.x, r.y, r.w, r.h
    );
}

void init_player(Player* p) {
    p->x = 600;
    p->y = 450;
    p->largura = 32;
    p->altura = 32;
    p->escala = 3;
    p->largura_scaled = p->largura * p->escala;
    p->altura_scaled = p->altura * p->escala;

    p->vida = VIDA_MAXIMA_PLAYER;
    p->invulneravel_timer = 0;
    p->velocidade = 5;

    p->cima = p->baixo = p->esquerda = p->direita = 0;
    p->coluna = p->linha = 0;
    p->contador_animacao = 0;

    p->atacando = 0;
    p->frame_ataque = 0;
    p->contador_ataque = 0;
    p->ultima_direcao = 0;
    p->ataque_acertou = 0;

    p->hit_x = 30;
    p->hit_y = 14;
    p->hit_w = 36;
    p->hit_h = 62;
}

void init_slimes(Slime slimes[]) {
    // Slime 1
    slimes[0].x = 300; slimes[0].y = 300;
    slimes[0].velocidade = 1.0f; slimes[0].direcao = 1;
    slimes[0].frame = 0; slimes[0].contador = 0;
    slimes[0].limite_esquerda = 200; slimes[0].limite_direita = 450;
    slimes[0].vida = VIDA_MAXIMA_SLIME; slimes[0].dano_timer = 0;
    slimes[0].hit_x = 16; slimes[0].hit_y = 20; slimes[0].hit_w = 32; slimes[0].hit_h = 28;

    // Slime 2
    slimes[1].x = 850; slimes[1].y = 500;
    slimes[1].velocidade = 1.5f; slimes[1].direcao = -1;
    slimes[1].frame = 0; slimes[1].contador = 15;
    slimes[1].limite_esquerda = 700; slimes[1].limite_direita = 1050;
    slimes[1].vida = VIDA_MAXIMA_SLIME; slimes[1].dano_timer = 0;
    slimes[1].hit_x = 16; slimes[1].hit_y = 20; slimes[1].hit_w = 32; slimes[1].hit_h = 28;
}

void atualizar_player(Player* p, Slime slimes[], int n_slimes,
    Retangulo obst[], int n_obst, Retangulo area) {

    float novo_x = p->x;
    float novo_y = p->y;

    // Movimento
    if (!p->atacando) {
        if (p->cima) { novo_y -= p->velocidade; p->linha = 5; p->ultima_direcao = 2; }
        else if (p->baixo) { novo_y += p->velocidade; p->linha = 3; p->ultima_direcao = 0; }
        else if (p->direita) { novo_x += p->velocidade; p->linha = 4; p->ultima_direcao = 1; }
        else if (p->esquerda) { novo_x -= p->velocidade; p->linha = 4; p->ultima_direcao = 3; }
    }

    // Limites da área (tela no exterior, sala no interior), baseados na hitbox
    if (novo_x + p->hit_x < area.x) novo_x = area.x - p->hit_x;
    if (novo_x + p->hit_x + p->hit_w > area.x + area.w) novo_x = area.x + area.w - p->hit_x - p->hit_w;
    if (novo_y + p->hit_y < area.y) novo_y = area.y - p->hit_y;
    if (novo_y + p->hit_y + p->hit_h > area.y + area.h) novo_y = area.y + area.h - p->hit_y - p->hit_h;

    // Colisões
    int pode_mover_x = 1, pode_mover_y = 1;

    if (!p->atacando) {
        // Slimes
        for (int i = 0; i < n_slimes; i++) {
            if (slimes[i].vida <= 0) continue;

            if (colisao_player_slime(p, &slimes[i], novo_x, p->y)) {
                pode_mover_x = 0;
                if (p->invulneravel_timer == 0 && p->vida > 0) {
                    p->vida--;
                    p->invulneravel_timer = TEMPO_INVULNERAVEL;
                }
            }
            if (colisao_player_slime(p, &slimes[i], p->x, novo_y)) {
                pode_mover_y = 0;
                if (p->invulneravel_timer == 0 && p->vida > 0) {
                    p->vida--;
                    p->invulneravel_timer = TEMPO_INVULNERAVEL;
                }
            }
        }

        // Obstáculos sólidos (casa, etc.)
        for (int j = 0; j < n_obst; j++) {
            if (colisao_player_rect(p, obst[j], novo_x, p->y)) pode_mover_x = 0;
            if (colisao_player_rect(p, obst[j], p->x, novo_y)) pode_mover_y = 0;
        }
    }

    if (pode_mover_x) p->x = novo_x;
    if (pode_mover_y) p->y = novo_y;

    if (p->invulneravel_timer > 0) p->invulneravel_timer--;

    // Animação de caminhada
    if (!p->atacando && (p->cima || p->baixo || p->esquerda || p->direita)) {
        p->contador_animacao++;
        if (p->contador_animacao >= 8) {
            p->coluna = (p->coluna + 1) % 6;
            p->contador_animacao = 0;
        }
    }

    // Lógica do Ataque
    if (p->atacando) {
        p->contador_ataque++;

        if (p->contador_ataque == 4 && !p->ataque_acertou) {
            float atq_x = p->x, atq_y = p->y;
            float atq_w = p->largura_scaled, atq_h = p->altura_scaled;

            if (p->ultima_direcao == 1) { atq_x += p->largura_scaled - 10; atq_w = 55; }
            else if (p->ultima_direcao == 3) { atq_x -= 45; atq_w = 55; }
            else if (p->ultima_direcao == 2) { atq_y -= 45; atq_h = 55; }
            else if (p->ultima_direcao == 0) { atq_y += p->altura_scaled - 10; atq_h = 55; }

            for (int i = 0; i < n_slimes; i++) {
                if (slimes[i].vida <= 0) continue;

                if (colisao(atq_x, atq_y, atq_w, atq_h, slimes[i].x + slimes[i].hit_x, slimes[i].y + slimes[i].hit_y, slimes[i].hit_w, slimes[i].hit_h)) {
                    slimes[i].vida--;
                    slimes[i].dano_timer = 12;
                    p->ataque_acertou = 1;
                    break;
                }
            }
        }

        if (p->contador_ataque >= 8) {
            p->frame_ataque++;
            p->contador_ataque = 0;
            if (p->frame_ataque >= 2) {
                p->atacando = 0;
                p->frame_ataque = 0;
                p->coluna = 0;
            }
        }
    }
}

void atualizar_slimes(Slime slimes[], Player* p) {
    for (int i = 0; i < NUM_SLIMES; i++) {
        if (slimes[i].vida <= 0) continue;

        float prox_x = slimes[i].x + slimes[i].velocidade * slimes[i].direcao;
        Slime teste = slimes[i];
        teste.x = prox_x;

        if (colisao_player_slime(p, &teste, p->x, p->y)) {
            slimes[i].direcao *= -1;
        }
        else {
            slimes[i].x = prox_x;
        }

        if (slimes[i].x <= slimes[i].limite_esquerda) {
            slimes[i].x = slimes[i].limite_esquerda;
            slimes[i].direcao = 1;
        }
        if (slimes[i].x >= slimes[i].limite_direita) {
            slimes[i].x = slimes[i].limite_direita;
            slimes[i].direcao = -1;
        }

        slimes[i].contador++;
        if (slimes[i].contador >= 10) {
            slimes[i].frame = (slimes[i].frame + 1) % 4;
            slimes[i].contador = 0;
        }

        if (slimes[i].dano_timer > 0) slimes[i].dano_timer--;
    }
}

void desenhar_barra_vida(float x, float y, float largura, float altura, int vida_atual, int vida_maxima) {
    if (vida_atual <= 0) return;

    float pct = (float)vida_atual / vida_maxima;

    al_draw_filled_rectangle(x, y, x + largura, y + altura, al_map_rgb(100, 0, 0));
    al_draw_filled_rectangle(x, y, x + (largura * pct), y + altura, al_map_rgb(0, 230, 0));
    al_draw_rectangle(x, y, x + largura, y + altura, al_map_rgb(0, 0, 0), 1.0);
}

void desenhar_cenario(ALLEGRO_BITMAP* grama, ALLEGRO_BITMAP* caminho, ALLEGRO_BITMAP* agua,
    ALLEGRO_BITMAP* casa, ALLEGRO_BITMAP* arvore, ALLEGRO_BITMAP* arvore_peq,
    ALLEGRO_BITMAP* bau, Posicao2D arvores[], Posicao2D arvores_peq[]) {

    // Grama
    for (int yy = 0; yy < ALTURA_TELA; yy += 16) {
        for (int xx = 0; xx < LARGURA_TELA; xx += 16) {
            al_draw_bitmap(grama, xx, yy, 0);
        }
    }

    // Caminho
    for (int xx = 0; xx < LARGURA_TELA; xx += 16) {
        al_draw_bitmap(caminho, xx, 360, 0);
        al_draw_bitmap(caminho, xx, 376, 0);
    }

    // Lago
    for (int yy = 100; yy < 260; yy += 16) {
        for (int xx = 650; xx < 850; xx += 16) {
            al_draw_bitmap(agua, xx, yy, 0);
        }
    }

    // Casa
    al_draw_scaled_bitmap(
        casa, 0, 0, al_get_bitmap_width(casa), al_get_bitmap_height(casa),
        40, 60, al_get_bitmap_width(casa) * ESCALA_CASA, al_get_bitmap_height(casa) * ESCALA_CASA, 0
    );

    // Árvores Grandes
    int sw = al_get_bitmap_width(arvore), sh = al_get_bitmap_height(arvore);
    float aw = sw * ESCALA_ARVORE, ah = sh * ESCALA_ARVORE;
    for (int i = 0; i < NUM_ARVORES; i++) {
        al_draw_scaled_bitmap(arvore, 0, 0, sw, sh, arvores[i].x, arvores[i].y, aw, ah, 0);
    }

    // Árvores Pequenas
    float p = 48 * ESCALA_ARVORE_PEQ;
    for (int i = 0; i < NUM_ARVORES_PEQ; i++) {
        al_draw_scaled_bitmap(arvore_peq, 0, 0, 48, 48, arvores_peq[i].x, arvores_peq[i].y, p, p, 0);
    }

    // Baú
    al_draw_scaled_bitmap(bau, 0, 0, 16, 16, 520, 300, 32, 32, 0);
}

void desenhar_interior(void) {
    // Piso
    al_draw_filled_rectangle(240, 120, 1040, 600, al_map_rgb(139, 98, 60));
    // Parede de fundo
    al_draw_filled_rectangle(240, 60, 1040, 120, al_map_rgb(90, 60, 40));
    // Contorno
    al_draw_rectangle(240, 60, 1040, 600, al_map_rgb(30, 20, 10), 4.0);
    // Porta de saída
    al_draw_filled_rectangle(600, 580, 680, 600, al_map_rgb(60, 30, 10));
}

void desenhar_hud(int slot_selecionado) {
    int largura_total = (NUM_SLOTS * TAMANHO_SLOT) + ((NUM_SLOTS - 1) * ESPACO_SLOT);
    int inicio_x = (LARGURA_TELA - largura_total) / 2;
    int inicio_y = ALTURA_TELA - 70;

    for (int i = 0; i < NUM_SLOTS; i++) {
        int slot_x = inicio_x + (i * (TAMANHO_SLOT + ESPACO_SLOT));

        al_draw_filled_rectangle(slot_x, inicio_y, slot_x + TAMANHO_SLOT, inicio_y + TAMANHO_SLOT, al_map_rgba(40, 40, 40, 200));

        if (i == slot_selecionado) {
            al_draw_rectangle(slot_x - 3, inicio_y - 3, slot_x + TAMANHO_SLOT + 3, inicio_y + TAMANHO_SLOT + 3, al_map_rgb(255, 255, 255), 3.0);
        }
        else {
            al_draw_rectangle(slot_x, inicio_y, slot_x + TAMANHO_SLOT, inicio_y + TAMANHO_SLOT, al_map_rgb(20, 20, 20), 2.0);
        }
    }
}