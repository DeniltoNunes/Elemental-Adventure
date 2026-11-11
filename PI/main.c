#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>



// ============================================================
// CONFIGURAÇÕES E CONSTANTES
// ============================================================

#define LARGURA_TELA 1280
#define ALTURA_TELA  720

#define NUM_ARVORES  4
#define NUM_ARVORES_PEQ 3

// Escalas do cenário
#define ESCALA_ARVORE       2.0f
#define ESCALA_ARVORE_PEQ   3.0f
#define ESCALA_CASA         3.0f
#define ESCALA_MOVEIS       2.0f   // escala dos móveis/tiles do interior

// Atributos de Vida
#define VIDA_MAXIMA_PLAYER 5
#define VIDA_MAXIMA_SLIME  2
#define TEMPO_INVULNERAVEL 60 // 1 segundo (60 frames)
#define RAIO_PERSEGUIR_SLIME 260.0f // distância em que o slime começa a perseguir o jogador

// Configurações do Inventário HUD
#define NUM_SLOTS    9
#define TAMANHO_SLOT 50
#define ESPACO_SLOT  8

// Configurações das Barras de Vida Flutuantes
#define LARGURA_BARRA_PLAYER 60.0f
#define ALTURA_BARRA_PLAYER  8.0f
#define LARGURA_BARRA_SLIME  40.0f
#define ALTURA_BARRA_SLIME   6.0f

// Interior da casa
#define NUM_MOVEIS 10

// Exterior: tiles do chão (16px desenhados em 48px = escala 3, igual à casa e ao jogador)
#define ESCALA_TILE      3
#define TILE             (16 * ESCALA_TILE)

// Caminho (3 tiles de altura, atravessa a tela inteira)
#define CAMINHO_Y        384
#define CAMINHO_LINHAS   3

// Lago
#define LAGO_X           672
#define LAGO_Y           96
#define LAGO_COLS        6
#define LAGO_LINHAS      4

// Curral dos bichos (cerca)
#define CURRAL_X         1008
#define CURRAL_Y         192
#define CURRAL_COLS      5
#define CURRAL_LINHAS    4

#define NUM_ANIMAIS      4
#define MAX_SOLIDOS      256   // (era 64)   // máximo de objetos sólidos por cena
#define MAX_SPRITES      64    // máximo de sprites fixos do cenário exterior

// Postes de luz e baú do exterior
#define POSTE_ESQ_X      350
#define POSTE_DIR_X      880
#define POSTE_Y          (CAMINHO_Y - 90)
#define BAU_X            520
#define BAU_Y            300

#define SALA_X     240   // canto esquerdo da sala
#define SALA_PAREDE_Y 56 // topo da parede de fundo (2 tiles de 32px)
#define SALA_CHAO_Y   120 // onde começa o piso
#define SALA_LARGURA  800
#define SALA_BASE_Y   600 // fim do piso


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
    int usando_picareta; // 1 = o golpe atual é na picareta (tecla Q); 0 = espada (tecla E)
    ALLEGRO_BITMAP* sprite_acoes; // folha Player_Actions.png (animação da picareta)

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
    ALLEGRO_BITMAP* spr;   // folha 64x64: 2 colunas x 2 linhas de 32x32 (linha 0 = parado, 1 = andando)
    float x, y, vel;
    int dir;               // 1 = direita, -1 = esquerda
    int frame, cont;
    int andando, fase;
    int lim_esq, lim_dir;

    // Hitbox (em volta dos pés, relativa a x e y)
    int hit_x, hit_y, hit_w, hit_h;
} Animal;

typedef struct {
    float x, y;
} Posicao2D;

typedef struct {
    float x, y, w, h;
} Retangulo;

// Qualquer coisa que bloqueia a passagem.
// Objeto parado: seg_x/seg_y são NULL e (x, y) é a posição na tela.
// Objeto que se move: seg_x/seg_y apontam para a posição dele e (x, y) é o deslocamento da hitbox.
typedef struct {
    float x, y, w, h;
    const float* seg_x;
    const float* seg_y;
} ObjetoSolido;

// Todos os objetos sólidos de uma cena
typedef struct {
    ObjetoSolido itens[MAX_SOLIDOS];
    int n;
} ListaSolidos;

typedef struct {
    ALLEGRO_BITMAP* bmp;
    float x, y, w, h;   // posição e tamanho já com escala
    int plano;          // 1 = sempre no fundo (janela, tapete, capacho)
    int solido;         // 1 = bloqueia o jogador
} Movel;

// Sprite fixo do cenário. base_y é a linha do "pé" do objeto: quem tem o pé
// mais acima na tela é desenhado primeiro e fica atrás dos outros.
typedef struct {
    ALLEGRO_BITMAP* bmp;
    float sx, sy, sw, sh;   // pedaço da folha de sprites
    float dx, dy, escala;   // onde desenhar na tela
    float base_y;
} SpriteCenario;

typedef struct {
    SpriteCenario itens[MAX_SPRITES];
    int n;
} ListaCenario;

// Uma entrada da fila de desenho do exterior
typedef enum { ITEM_CENARIO, ITEM_ANIMAL, ITEM_PLAYER } TipoItem;

typedef struct {
    float base_y;
    TipoItem tipo;
    int indice;
} ItemDesenho;

typedef enum { CENA_EXTERIOR, CENA_INTERIOR, CENA_CAVERNA } Cena;


// ============================================================
// PROTÓTIPOS DAS FUNÇÕES
// ============================================================

int colisao(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);
int colisao_player_slime(Player* p, Slime* s, float novo_x, float novo_y);
int colisao_pes_rect(Player* p, Retangulo r, float novo_x, float novo_y);

void adicionar_solido(ListaSolidos* lista, float x, float y, float w, float h);
void adicionar_solido_movel(ListaSolidos* lista, const float* x, const float* y, float hit_x, float hit_y, float w, float h);
Retangulo caixa_solido(const ObjetoSolido* s);
int ha_solido_em(const ListaSolidos* lista, float x, float y, float w, float h, const float* ignorar);

void init_player(Player* p);

void atualizar_player(Player* p, Slime slimes[], int n_slimes,
    const ListaSolidos* solidos, Retangulo area);
void atualizar_slimes(Slime slimes[], int n_slimes, Player* p, const ListaSolidos* solidos, Retangulo area);
void iniciar_golpe(Player* p, int picareta);
Retangulo area_golpe(const Player* p);
void reviver_player(Player* p);

void adicionar_movel(Movel lista[], int* n, ALLEGRO_BITMAP* bmp, float x, float y, int plano, int solido);

void desenhar_barra_vida(float x, float y, float largura, float altura, int vida_atual, int vida_maxima);
void atualizar_animais(Animal animais[], Player* p, const ListaSolidos* solidos);
void desenhar_animal(Animal* a);
void desenhar_slime(Slime* s, ALLEGRO_BITMAP* sprite);
void desenhar_player(Player* p, ALLEGRO_BITMAP* sprite);

ALLEGRO_BITMAP* gerar_fundo_exterior(ALLEGRO_DISPLAY* janela, ALLEGRO_BITMAP* grama, ALLEGRO_BITMAP* caminho,
    ALLEGRO_BITMAP* agua, ALLEGRO_BITMAP* decor);
void adicionar_sprite(ListaCenario* lista, ALLEGRO_BITMAP* bmp, float sx, float sy, float sw, float sh,
    float dx, float dy, float escala, float base_y);
void montar_cenario(ListaCenario* lista, ALLEGRO_BITMAP* casa, ALLEGRO_BITMAP* arvore,
    ALLEGRO_BITMAP* arvore_peq, ALLEGRO_BITMAP* bau, ALLEGRO_BITMAP* decor, ALLEGRO_BITMAP* cerca,
    Posicao2D arvores[], Posicao2D arvores_peq[], float base_casa);
void desenhar_exterior(const ListaCenario* cenario, Animal animais[], Player* p, ALLEGRO_BITMAP* personagem);
void desenhar_interior_fundo(ALLEGRO_BITMAP* piso, ALLEGRO_BITMAP* parede_topo, ALLEGRO_BITMAP* parede_base);
void desenhar_moveis(Movel moveis[], int n, float pes_y, int depois_do_player);
void desenhar_moveis(Movel moveis[], int n, float pes_y, int depois_do_player);
void desenhar_moveis(Movel moveis[], int n, float pes_y, int depois_do_player);

// Declaração da versão com 3 argumentos
void desenhar_hud(int slot_selecionado, ALLEGRO_BITMAP* icone_ferro, int qtd_ferro);


// ============================================================
// FASE 2: CAVERNA DE MINÉRIO DE FERRO
// Incluir DEPOIS dos typedefs e protótipos do jogo e ANTES do main().
// ============================================================

#define CAV_NUM_SLIMES     4
#define CAV_NUM_VEIOS      6                 // veios de minério espalhados pelo chão
#define CAV_META_MINERIO   CAV_NUM_VEIOS
#define CAV_VIDA_VEIO      3                 // golpes de picareta (tecla Q) para quebrar cada veio
#define CAV_ESC_VEIO       4                 // veio: grade 16x16 desenhada em 64x64
#define CAV_MAX_DECOR      16
#define CAV_MAX_POP        8
#define CAV_CEL            16                // tamanho da célula da parede de pedra
#define CAV_COLS           (LARGURA_TELA / CAV_CEL)
#define CAV_LINHAS         (ALTURA_TELA / CAV_CEL)
#define CAV_PASTA          "assets_cave/Objects_separately/"

enum {
    CAV_IMG_ROCHA_G, CAV_IMG_ROCHA_M, CAV_IMG_ROCHA_P,
    CAV_IMG_CRISTAL_M, CAV_IMG_CRISTAL_P,
    CAV_IMG_COGUMELO, CAV_IMG_FOGUEIRA, CAV_IMG_ESQUELETO,
    CAV_NUM_IMGS
};

typedef struct {
    ALLEGRO_BITMAP* bmp;
    float x, y, w, h;
    float base_y;       // linha do "pé" para ordenar a profundidade
} CavDecor;

// Veio de minério de ferro no chão: o jogador bate com a picareta (Q) até quebrar
typedef struct {
    float x, y, w, h;
    int vida;           // golpes restantes
    int ativo;          // 1 = ainda no chão
    int idx_solido;     // posição na lista de sólidos (para liberar a passagem ao quebrar)
    float sol_x;        // x original da hitbox (para restaurar no reset)
    int flash, tremor;  // efeitos ao levar um golpe
} CavVeio;

typedef struct { float x, y; int t; } CavPopup;   // ícone que sobe quando o minério é coletado

typedef struct {
    ALLEGRO_BITMAP* fundo;               // chão + paredes de pedra pré-desenhados
    ALLEGRO_BITMAP* minerio;             // ícone do minério 12x12 (HUD e inventário)
    ALLEGRO_BITMAP* veio_bmp;            // sprite do veio 16x16 (gerado por código)
    ALLEGRO_BITMAP* img[CAV_NUM_IMGS];   // objetos da pasta assets_cave

    CavDecor decor[CAV_MAX_DECOR];
    int n_decor;

    Slime slimes[CAV_NUM_SLIMES];
    CavVeio veios[CAV_NUM_VEIOS];
    CavPopup pop[CAV_MAX_POP];
    int coletados;                       // minérios no inventário

    unsigned char rocha[CAV_LINHAS][CAV_COLS];   // 1 = célula de pedra (parede), 0 = chão

    ListaSolidos solidos;
    Retangulo area;    // área permitida para a HITBOX do jogador
    Retangulo saida;   // boca da caverna (placa) para voltar ao mapa
} Caverna;

typedef enum { CAV_ITEM_DECOR, CAV_ITEM_VEIO, CAV_ITEM_SLIME, CAV_ITEM_PLAYER } CavTipoItem;
typedef struct { float base_y; CavTipoItem tipo; int indice; } CavItem;


// ------------------------------------------------------------
// PLACA de passagem entre as fases (no lugar da antiga luz)
// ------------------------------------------------------------
// r = área que muda de cena quando o jogador pisa nela; a placa é desenhada no meio dela
static void desenhar_placa(Retangulo r) {
    float cx = r.x + r.w / 2.0f;
    float pe = r.y + r.h / 2.0f + 24.0f;          // onde o poste encosta no chão

    ALLEGRO_COLOR madeira = al_map_rgb(150, 100, 52);
    ALLEGRO_COLOR escura  = al_map_rgb(86, 54, 26);

    al_draw_filled_ellipse(cx, pe, 16, 5, al_map_rgba(0, 0, 0, 90));                    // sombra
    al_draw_filled_rectangle(cx - 3, pe - 28, cx + 3, pe, escura);                      // poste
    al_draw_filled_rectangle(cx - 20, pe - 50, cx + 20, pe - 26, madeira);              // tábua
    al_draw_rectangle(cx - 20, pe - 50, cx + 20, pe - 26, escura, 2.0f);                // contorno
    al_draw_filled_rectangle(cx - 10, pe - 40, cx + 12, pe - 36, al_map_rgb(245, 235, 210));          // haste da seta
    al_draw_filled_triangle(cx - 16, pe - 38, cx - 8, pe - 45, cx - 8, pe - 31, al_map_rgb(245, 235, 210));   // ponta da seta
}

// ------------------------------------------------------------
// Ruído e geração do fundo da caverna
// ------------------------------------------------------------
static unsigned cav_hash(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

static float cav_aleat(int x, int y) {
    return (float)(cav_hash(x, y) & 0xFFFFu) / 65535.0f;
}

// Ruído suave (0..1): manchas grandes e pequenas, sempre iguais
static float cav_ruido(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = cav_aleat(ix, iy), b = cav_aleat(ix + 1, iy);
    float c = cav_aleat(ix, iy + 1), d = cav_aleat(ix + 1, iy + 1);
    float topo = a + (b - a) * fx;
    float base = c + (d - c) * fx;
    return topo + (base - topo) * fy;
}

static float cav_ruido1(float x, int sem) { return cav_ruido(x, (float)sem); }

// Diz se uma célula de 16x16 é parede de pedra. O MESMO mapa é usado para desenhar
// e para colidir, então o que você vê é exatamente o que bloqueia.
static int cav_calc_rocha(int cx, int cy) {
    // teto irregular (linhas 10 a 13)
    int topo = 10 + (int)(cav_ruido1(cx * 0.35f, 1) * 3.99f);
    if (cy < topo) return 1;

    // chão de baixo irregular
    int fundo = 40 - (int)(cav_ruido1(cx * 0.40f, 2) * 2.99f);
    if (cy >= fundo) return 1;

    // parede esquerda: boca da caverna aberta nas linhas 19..29 (a saída), com "ombros" de pedra
    int esq;
    if (cy >= 19 && cy <= 29) esq = 0;
    else if (cy == 17 || cy == 18 || cy == 30 || cy == 31) esq = 3;
    else esq = 1 + (int)(cav_ruido1(cy * 0.40f, 3) * 2.99f);
    if (cx < esq) return 1;

    // parede direita irregular
    int dir = 1 + (int)(cav_ruido1(cy * 0.40f, 4) * 2.99f);
    if (cx >= CAV_COLS - dir) return 1;

    // pilares / colunas de pedra (x, y, raio em pixels)
    static const float pil[3][3] = { { 800, 330, 30 },{ 1190, 450, 34 },{ 140, 540, 36 } };
    float px = cx * (float)CAV_CEL + CAV_CEL / 2.0f;
    float py = cy * (float)CAV_CEL + CAV_CEL / 2.0f;
    for (int k = 0; k < 3; k++) {
        float dx = px - pil[k][0], dy = py - pil[k][1];
        float r = pil[k][2] + (cav_ruido(cx * 0.45f + k * 17.0f, cy * 0.45f) - 0.5f) * 18.0f;
        if (dx * dx + dy * dy < r * r) return 1;
    }
    return 0;
}

static void cav_montar_rocha(Caverna* c) {
    for (int cy = 0; cy < CAV_LINHAS; cy++)
        for (int cx = 0; cx < CAV_COLS; cx++)
            c->rocha[cy][cx] = (unsigned char)cav_calc_rocha(cx, cy);
}

// Lê o mapa com segurança (fora da tela repete a célula da borda)
static int cav_r(const Caverna* c, int cx, int cy) {
    if (cx < 0) cx = 0;
    if (cx >= CAV_COLS) cx = CAV_COLS - 1;
    if (cy < 0) cy = 0;
    if (cy >= CAV_LINHAS) cy = CAV_LINHAS - 1;
    return c->rocha[cy][cx];
}

// Transforma as células de pedra em retângulos sólidos (juntando células vizinhas)
static void cav_solidos_da_rocha(Caverna* c) {
    unsigned char usado[CAV_LINHAS][CAV_COLS];
    for (int y = 0; y < CAV_LINHAS; y++)
        for (int x = 0; x < CAV_COLS; x++) usado[y][x] = 0;

    // só importa a região onde o jogador consegue chegar
    int x0 = 2, x1 = CAV_COLS - 3, y0 = 6, y1 = CAV_LINHAS - 6;

    for (int cy = y0; cy <= y1; cy++) {
        for (int cx = x0; cx <= x1; cx++) {
            if (!c->rocha[cy][cx] || usado[cy][cx]) continue;

            int ex = cx;
            while (ex + 1 <= x1 && c->rocha[cy][ex + 1] && !usado[cy][ex + 1]) ex++;

            int ey = cy;
            while (ey + 1 <= y1) {
                int ok = 1;
                for (int k = cx; k <= ex; k++) {
                    if (!c->rocha[ey + 1][k] || usado[ey + 1][k]) { ok = 0; break; }
                }
                if (!ok) break;
                ey++;
            }

            for (int yy = cy; yy <= ey; yy++)
                for (int xx = cx; xx <= ex; xx++) usado[yy][xx] = 1;

            adicionar_solido(&c->solidos, (float)(cx * CAV_CEL), (float)(cy * CAV_CEL),
                (float)((ex - cx + 1) * CAV_CEL), (float)((ey - cy + 1) * CAV_CEL));
        }
    }
}

static void cav_bloco(float x, float y, float w, float h, int tom) {
    if (tom < 4) tom = 4;
    if (tom > 250) tom = 250;
    al_draw_filled_rectangle(x, y, x + w, y + h, al_map_rgb(tom, tom - 3, tom + 4));
}

// Desenha UMA vez só a caverna inteira: chão de terra e pedra, paredes de rocha com
// estalactites, estalagmites, poças e pedrinhas
static ALLEGRO_BITMAP* cav_gerar_fundo(ALLEGRO_DISPLAY* janela, const Caverna* c) {
    ALLEGRO_BITMAP* fundo = al_create_bitmap(LARGURA_TELA, ALTURA_TELA);
    if (!fundo) return NULL;

    al_set_target_bitmap(fundo);
    al_clear_to_color(al_map_rgb(8, 7, 10));

    // ---- 1) chão e rocha (blocos de 4x4 px, estilo pixel art) ----
    for (int cy = 0; cy < CAV_LINHAS; cy++) {
        for (int cx = 0; cx < CAV_COLS; cx++) {
            float x = (float)(cx * CAV_CEL), y = (float)(cy * CAV_CEL);

            if (!c->rocha[cy][cx]) {
                // chão natural: terra e pedra em manchas irregulares, sem grade de ladrilhos
                for (int sy = 0; sy < 4; sy++) {
                    for (int sx = 0; sx < 4; sx++) {
                        float px = x + sx * 4, py = y + sy * 4;
                        float n = 0.55f * cav_ruido(px / 70.0f, py / 70.0f)
                            + 0.30f * cav_ruido(px / 24.0f + 11.0f, py / 24.0f + 7.0f)
                            + 0.15f * cav_aleat((int)px / 4, (int)py / 4);
                        int tom = 38 + (int)(n * 28.0f);
                        if (cav_ruido(px / 110.0f + 31.0f, py / 110.0f + 5.0f) > 0.66f) tom -= 9;   // terra escura
                        al_draw_filled_rectangle(px, py, px + 4, py + 4, al_map_rgb(tom + 8, tom + 2, tom - 4));
                    }
                }
                continue;
            }

            // rocha: quem tem chão ao lado/embaixo ganha "face" iluminada; o miolo fica quase preto
            int sul = !cav_r(c, cx, cy + 1), norte = !cav_r(c, cx, cy - 1);
            int oeste = !cav_r(c, cx - 1, cy), leste = !cav_r(c, cx + 1, cy);
            int d_sul = sul ? 1 : (!cav_r(c, cx, cy + 2) ? 2 : 0);
            int base = (d_sul == 1) ? 54 : (d_sul == 2) ? 36 : 19;

            for (int sx = 0; sx < 4; sx++) {
                int var = (int)((cav_hash(cx * 4 + sx, cy / 2 + 3) >> 3) % 17u) - 8;   // listras verticais
                for (int sy = 0; sy < 4; sy++) {
                    int gr = (int)((cav_hash(cx * 4 + sx, cy * 4 + sy + 99) >> 5) % 7u) - 3;
                    int tom = base + (d_sul ? var : var / 3) + gr;
                    cav_bloco(x + sx * 4, y + sy * 4, 4, 4, tom);
                }
            }

            if (norte) cav_bloco(x, y, 16, 4, 68);          // topo iluminado
            if (oeste) cav_bloco(x, y, 4, 16, 58);          // quina iluminada
            if (leste) cav_bloco(x + 12, y, 4, 16, 52);
            if (sul)   cav_bloco(x, y + 12, 16, 4, 22);     // sombra na base da parede

            unsigned h = cav_hash(cx, cy * 7 + 1);
            if (d_sul && (h % 9u) == 0) {                   // rachadura
                float rx = x + 2 + (float)((h >> 4) % 12u);
                al_draw_line(rx, y + 2, rx + 2, y + 12, al_map_rgb(18, 15, 22), 2.0f);
            }
            if (!d_sul && !norte && !oeste && !leste && (h % 19u) == 0) {   // cristal/minério na rocha funda
                float sx2 = x + 4.0f * (float)((h >> 8) % 4u);
                float sy2 = y + 4.0f * (float)((h >> 12) % 4u);
                int azul = ((h >> 16) & 1u) == 0;
                al_draw_filled_rectangle(sx2, sy2, sx2 + 4, sy2 + 4, azul ? al_map_rgb(28, 80, 82) : al_map_rgb(96, 52, 26));
                al_draw_filled_rectangle(sx2 + 1, sy2 + 1, sx2 + 3, sy2 + 3, azul ? al_map_rgb(70, 210, 200) : al_map_rgb(214, 122, 56));
            }
        }
    }

    // ---- 2) detalhes do chão: pedrinhas, rachaduras e poças ----
    for (int cy = 0; cy < CAV_LINHAS; cy++) {
        for (int cx = 0; cx < CAV_COLS; cx++) {
            if (c->rocha[cy][cx]) continue;
            unsigned h = cav_hash(cx * 3 + 1, cy * 5 + 2);
            float x = (float)(cx * CAV_CEL), y = (float)(cy * CAV_CEL);

            if ((h % 9u) == 0) {
                float px = x + 2 + (float)((h >> 4) % 10u), py = y + 2 + (float)((h >> 8) % 10u);
                al_draw_filled_rectangle(px, py + 2, px + 5, py + 4, al_map_rgb(24, 21, 20));   // sombra
                al_draw_filled_rectangle(px, py, px + 4, py + 3, al_map_rgb(88, 82, 76));       // pedrinha
            }
            if (((h >> 12) % 23u) == 0) {
                float px = x + 2 + (float)((h >> 4) % 8u), py = y + 2 + (float)((h >> 8) % 8u);
                al_draw_line(px, py, px + 9, py + 6, al_map_rgb(22, 18, 17), 2.0f);
            }
        }
    }

    static const float pocas[3][4] = { { 560, 560, 42, 14 },{ 1000, 330, 36, 12 },{ 260, 430, 34, 11 } };
    for (int i = 0; i < 3; i++) {
        float px = pocas[i][0], py = pocas[i][1], rx = pocas[i][2], ry = pocas[i][3];
        al_draw_filled_ellipse(px, py, rx, ry, al_map_rgb(22, 34, 46));
        al_draw_ellipse(px, py, rx, ry, al_map_rgb(66, 92, 108), 1.0f);
        al_draw_line(px - rx * 0.5f, py - ry * 0.3f, px - rx * 0.1f, py - ry * 0.5f, al_map_rgb(110, 140, 150), 1.0f);
    }

    // ---- 3) sombras no chão junto das paredes + borda irregular nas laterais ----
    for (int cy = 0; cy < CAV_LINHAS; cy++) {
        for (int cx = 0; cx < CAV_COLS; cx++) {
            if (c->rocha[cy][cx]) continue;
            float x = (float)(cx * CAV_CEL), y = (float)(cy * CAV_CEL);

            if (cav_r(c, cx, cy - 1)) {
                static const float a_topo[4] = { 0.50f, 0.34f, 0.20f, 0.09f };
                for (int i = 0; i < 4; i++)
                    al_draw_filled_rectangle(x, y + i * 4, x + 16, y + i * 4 + 4, al_map_rgba_f(0, 0, 0, a_topo[i]));
            }
            if (cav_r(c, cx - 1, cy)) {
                static const float a_lado[3] = { 0.38f, 0.20f, 0.08f };
                for (int i = 0; i < 3; i++)
                    al_draw_filled_rectangle(x + i * 4, y, x + i * 4 + 4, y + 16, al_map_rgba_f(0, 0, 0, a_lado[i]));
                for (int k = 0; k < 4; k++) {
                    unsigned h = cav_hash(cx * 3 + k, cy * 5 + 17);
                    if ((h % 4u) == 0) cav_bloco(x, y + k * 4, ((h >> 5) & 1u) ? 8.0f : 4.0f, 4, 44);
                }
            }
            if (cav_r(c, cx + 1, cy)) {
                static const float a_lado[3] = { 0.38f, 0.20f, 0.08f };
                for (int i = 0; i < 3; i++)
                    al_draw_filled_rectangle(x + 16 - i * 4 - 4, y, x + 16 - i * 4, y + 16, al_map_rgba_f(0, 0, 0, a_lado[i]));
                for (int k = 0; k < 4; k++) {
                    unsigned h = cav_hash(cx * 7 + k, cy * 5 + 41);
                    if ((h % 4u) == 0) {
                        float w = ((h >> 5) & 1u) ? 8.0f : 4.0f;
                        cav_bloco(x + 16 - w, y + k * 4, w, 4, 44);
                    }
                }
            }
        }
    }

    // ---- 4) estalactites (descem do teto) e estalagmites (sobem do chão) ----
    for (int cy = 0; cy < CAV_LINHAS; cy++) {
        for (int cx = 0; cx < CAV_COLS; cx++) {
            if (!c->rocha[cy][cx]) continue;
            float x = (float)(cx * CAV_CEL), y = (float)(cy * CAV_CEL);

            if (!cav_r(c, cx, cy + 1)) {
                unsigned h = cav_hash(cx * 5 + 2, cy * 3 + 9);
                if ((h % 4u) == 0) {
                    float px = x + 3 + (float)((h >> 4) % 10u);
                    float len = 8 + (float)((h >> 9) % 20u);
                    float meia = 3 + (float)((h >> 14) % 3u);
                    float ponta = px + (float)((int)((h >> 17) % 3u) - 1);
                    al_draw_filled_triangle(px - meia, y + 15, px + meia, y + 15, ponta, y + 16 + len, al_map_rgb(50, 46, 56));
                    al_draw_line(px - meia + 1, y + 15, ponta, y + 16 + len, al_map_rgb(80, 74, 86), 1.0f);
                }
            }
            if (!cav_r(c, cx, cy - 1)) {
                unsigned h = cav_hash(cx * 11 + 5, cy * 7 + 3);
                if ((h % 5u) == 0) {
                    float px = x + 3 + (float)((h >> 4) % 10u);
                    float len = 6 + (float)((h >> 9) % 11u);
                    float meia = 3 + (float)((h >> 14) % 3u);
                    al_draw_filled_triangle(px - meia, y + 1, px + meia, y + 1, px, y - len, al_map_rgb(70, 66, 76));
                    al_draw_line(px - meia + 1, y + 1, px, y - len, al_map_rgb(104, 98, 110), 1.0f);
                }
            }
        }
    }

    al_set_target_backbuffer(janela);
    return fundo;
}

// Minério de ferro (ícone do inventário): pixel art 12x12 cinza com veios cor de ferrugem
static ALLEGRO_BITMAP* cav_gerar_minerio(ALLEGRO_DISPLAY* janela) {
    static const char* grade[12] = {
        "....kkkk....",
        "..kkggggkk..",
        ".kggggrgggk.",
        ".kgggggoggk.",
        "kgdggrrgggdk",
        "kggggroggggk",
        "kgrggggggdgk",
        "kgorggdgggrk",
        ".kdgggggrogk",
        ".kkdgggggdkk",
        "..kkddddkk..",
        "....kkkk...."
    };

    ALLEGRO_BITMAP* bmp = al_create_bitmap(12, 12);
    if (!bmp) return NULL;

    al_set_target_bitmap(bmp);
    al_clear_to_color(al_map_rgba(0, 0, 0, 0));

    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < 12; x++) {
            ALLEGRO_COLOR c;
            switch (grade[y][x]) {
            case 'k': c = al_map_rgb(30, 28, 34);    break;
            case 'g': c = al_map_rgb(112, 112, 124); break;
            case 'd': c = al_map_rgb(72, 72, 84);    break;
            case 'r': c = al_map_rgb(190, 95, 45);   break;
            case 'o': c = al_map_rgb(236, 146, 74);  break;
            default: continue;
            }
            al_put_pixel(x, y, c);
        }
    }

    al_set_target_backbuffer(janela);
    return bmp;
}

// Veio de minério no chão: pedra com filões de ferro (16x16, desenhado em 64x64)
static ALLEGRO_BITMAP* cav_gerar_veio(ALLEGRO_DISPLAY* janela) {
    static const char* grade[16] = {
        "................",
        "......kkkk......",
        "....kkhhhgkk....",
        "...khhggrggdk...",
        "..khgggrogggdk..",
        ".khggrgggggoddk.",
        ".kggroggggggrdk.",
        "kgggrrgggorgggdk",
        "kggggggrogggrgdk",
        "kgorggggggggrodk",
        "kgrgggdggggggrdk",
        "kdgggggggorgggdk",
        ".kdgggrgggggddk.",
        ".kkddggggggddkk.",
        "..kkkddddddkkk..",
        "................"
    };

    ALLEGRO_BITMAP* bmp = al_create_bitmap(16, 16);
    if (!bmp) return NULL;

    al_set_target_bitmap(bmp);
    al_clear_to_color(al_map_rgba(0, 0, 0, 0));

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            ALLEGRO_COLOR c;
            switch (grade[y][x]) {
            case 'k': c = al_map_rgb(26, 24, 30);    break;  // contorno
            case 'g': c = al_map_rgb(104, 104, 116); break;  // rocha
            case 'h': c = al_map_rgb(150, 150, 162); break;  // brilho da rocha
            case 'd': c = al_map_rgb(66, 66, 78);    break;  // sombra
            case 'r': c = al_map_rgb(184, 90, 42);   break;  // ferrugem
            case 'o': c = al_map_rgb(238, 148, 76);  break;  // brilho do ferro
            default: continue;
            }
            al_put_pixel(x, y, c);
        }
    }

    al_set_target_backbuffer(janela);
    return bmp;
}


// ------------------------------------------------------------
// Montagem da fase
// ------------------------------------------------------------
// cx = centro do objeto, base_y = onde ele "pisa" no chão
static void cav_add_decor(Caverna* c, int img, float cx, float base_y, float escala, int solido) {
    if (c->n_decor >= CAV_MAX_DECOR) return;

    CavDecor* d = &c->decor[c->n_decor++];
    d->bmp = c->img[img];
    d->w = al_get_bitmap_width(d->bmp) * escala;
    d->h = al_get_bitmap_height(d->bmp) * escala;
    d->x = cx - d->w / 2.0f;
    d->y = base_y - d->h * 0.92f;

    if (solido) {
        d->base_y = base_y;
        // hitbox só na base do objeto
        adicionar_solido(&c->solidos, d->x + d->w * 0.25f, d->y + d->h * 0.62f, d->w * 0.5f, d->h * 0.30f);
    }
    else {
        d->base_y = d->y + d->h * 0.4f;   // objeto "no chão": fica sempre atrás do jogador
    }
}

static void cav_add_veio(Caverna* c, int i, float x, float y) {
    CavVeio* v = &c->veios[i];
    v->x = x; v->y = y;
    v->w = 16.0f * CAV_ESC_VEIO;
    v->h = 16.0f * CAV_ESC_VEIO;
    v->sol_x = x + v->w * 0.12f;

    int antes = c->solidos.n;
    adicionar_solido(&c->solidos, v->sol_x, y + v->h * 0.45f, v->w * 0.76f, v->h * 0.45f);
    v->idx_solido = (c->solidos.n > antes) ? antes : -1;
}

static void cav_init_slime(Slime* s, float x, float y, float vel, int lim_esq, int lim_dir) {
    s->x = x; s->y = y;
    s->velocidade = vel; s->direcao = 1;
    s->frame = 0; s->contador = 0;
    s->limite_esquerda = lim_esq; s->limite_direita = lim_dir;
    s->vida = VIDA_MAXIMA_SLIME; s->dano_timer = 0;
    s->hit_x = 16; s->hit_y = 20; s->hit_w = 32; s->hit_h = 28;
}

// Volta a fase ao começo (usado ao entrar pela primeira vez e se o jogador morrer)
static void cav_resetar(Caverna* c) {
    cav_init_slime(&c->slimes[0], 420, 300, 1.0f, 330, 620);
    cav_init_slime(&c->slimes[1], 760, 470, 1.2f, 620, 900);
    cav_init_slime(&c->slimes[2], 960, 270, 1.0f, 880, 1150);
    cav_init_slime(&c->slimes[3], 980, 520, 1.4f, 900, 1170);

    for (int i = 0; i < CAV_NUM_VEIOS; i++) {
        CavVeio* v = &c->veios[i];
        v->vida = CAV_VIDA_VEIO;
        v->ativo = 1;
        v->flash = 0;
        v->tremor = 0;
        if (v->idx_solido >= 0) c->solidos.itens[v->idx_solido].x = v->sol_x;   // volta a bloquear
    }
    for (int i = 0; i < CAV_MAX_POP; i++) c->pop[i].t = 0;
    c->coletados = 0;
}

static int cav_carregar(Caverna* c, ALLEGRO_DISPLAY* janela) {
    static const char* arquivos[CAV_NUM_IMGS] = {
        CAV_PASTA "128/Beige_rpck_dark_shadow1.png",
        CAV_PASTA "64/Beige_rpck_dark_shadow2.png",
        CAV_PASTA "64/Beige_rpck_dark_shadow3.png",
        CAV_PASTA "64/Blue-green_crystal_dark_shadow1.png",
        CAV_PASTA "32/Blue-green_crystal_dark_shadow3.png",
        CAV_PASTA "64/mushroom1_dark_shadow1.png",
        CAV_PASTA "64/Bonefire_dark_shadow.png",
        CAV_PASTA "32/Human_skeleton_dark_shadow.png"
    };

    for (int i = 0; i < CAV_NUM_IMGS; i++) {
        c->img[i] = al_load_bitmap(arquivos[i]);
        if (!c->img[i]) {
            printf("ERRO: nao foi possivel carregar %s\n", arquivos[i]);
            return 0;
        }
    }

    c->area = (Retangulo){ 40, 100, 1200, 540 };     // limites da HITBOX do jogador
    c->saida = (Retangulo){ 0, 300, 70, 180 };       // boca da caverna (esquerda)
    c->solidos.n = 0;
    c->n_decor = 0;

    // Paredes de pedra: o mesmo mapa gera o desenho e a colisão
    cav_montar_rocha(c);
    cav_solidos_da_rocha(c);

    c->fundo = cav_gerar_fundo(janela, c);
    c->minerio = cav_gerar_minerio(janela);
    c->veio_bmp = cav_gerar_veio(janela);
    if (!c->fundo || !c->minerio || !c->veio_bmp) {
        printf("ERRO: nao foi possivel gerar o fundo/minerio da caverna.\n");
        return 0;
    }

    // Objetos da caverna (centro x, base y, escala, sólido?)
    cav_add_decor(c, CAV_IMG_ROCHA_M, 330, 262, 1.5f, 1);
    cav_add_decor(c, CAV_IMG_CRISTAL_M, 600, 255, 1.5f, 1);
    cav_add_decor(c, CAV_IMG_ROCHA_G, 930, 268, 1.0f, 1);
    cav_add_decor(c, CAV_IMG_COGUMELO, 1150, 275, 1.5f, 1);
    cav_add_decor(c, CAV_IMG_CRISTAL_P, 240, 585, 2.0f, 1);
    cav_add_decor(c, CAV_IMG_ROCHA_P, 560, 590, 1.5f, 1);
    cav_add_decor(c, CAV_IMG_COGUMELO, 800, 585, 1.5f, 1);
    cav_add_decor(c, CAV_IMG_CRISTAL_M, 1060, 580, 1.5f, 1);
    cav_add_decor(c, CAV_IMG_ESQUELETO, 420, 440, 2.0f, 0);
    cav_add_decor(c, CAV_IMG_FOGUEIRA, 220, 300, 1.5f, 0);

    // Veios de minério de ferro no chão (x, y = canto superior esquerdo do sprite 64x64)
    static const float pos_veios[CAV_NUM_VEIOS][2] = {
        { 300, 430 },{ 540, 400 },{ 650, 340 },{ 880, 400 },{ 1040, 360 },{ 340, 520 }
    };
    for (int i = 0; i < CAV_NUM_VEIOS; i++) cav_add_veio(c, i, pos_veios[i][0], pos_veios[i][1]);

    cav_resetar(c);
    return 1;
}

static void cav_destruir(Caverna* c) {
    if (!c) return;
    for (int i = 0; i < CAV_NUM_IMGS; i++) {
        if (c->img[i]) al_destroy_bitmap(c->img[i]);
        c->img[i] = NULL;
    }
    if (c->fundo) al_destroy_bitmap(c->fundo);
    if (c->minerio) al_destroy_bitmap(c->minerio);
    if (c->veio_bmp) al_destroy_bitmap(c->veio_bmp);
    c->fundo = c->minerio = c->veio_bmp = NULL;
}


// ------------------------------------------------------------
// Atualização
// ------------------------------------------------------------
// Alcance da picareta: um veio é acertado se estiver a até CAV_ALCANCE_PICARETA pixels
// dos pés do jogador, em qualquer lado.
#define CAV_ALCANCE_PICARETA 40.0f

// Devolve o veio mais próximo ao alcance do jogador (-1 se não houver nenhum)
static int cav_veio_no_alcance(const Caverna* c, const Player* p) {
    float a = CAV_ALCANCE_PICARETA;
    float bx = p->x + p->hit_x - a, by = p->y + p->hit_y + p->hit_h - 20 - a;
    float bw = p->hit_w + 2 * a,    bh = 20 + 2 * a;
    float pcx = p->x + p->hit_x + p->hit_w / 2.0f, pcy = p->y + p->hit_y + p->hit_h / 2.0f;
    int melhor = -1;
    float melhor_d = 1e18f;

    for (int i = 0; i < CAV_NUM_VEIOS; i++) {
        const CavVeio* v = &c->veios[i];
        if (!v->ativo) continue;
        if (!colisao(bx, by, bw, bh, v->x + 4, v->y + 4, v->w - 8, v->h - 8)) continue;

        float dx = v->x + v->w / 2.0f - pcx, dy = v->y + v->h / 2.0f - pcy;
        float d = dx * dx + dy * dy;
        if (d < melhor_d) { melhor_d = d; melhor = i; }
    }
    return melhor;
}

// Vira o jogador de frente para o veio que vai ser picado
static void cav_preparar_picareta(const Caverna* c, Player* p) {
    int i = cav_veio_no_alcance(c, p);
    if (i < 0) return;

    float dx = c->veios[i].x + c->veios[i].w / 2.0f - (p->x + p->hit_x + p->hit_w / 2.0f);
    float dy = c->veios[i].y + c->veios[i].h / 2.0f - (p->y + p->hit_y + p->hit_h / 2.0f);

    if (fabsf(dx) > fabsf(dy)) { p->ultima_direcao = (dx > 0) ? 1 : 3; p->linha = 4; }
    else if (dy > 0)           { p->ultima_direcao = 0;                 p->linha = 3; }
    else                       { p->ultima_direcao = 2;                 p->linha = 5; }
}

// Um golpe de picareta: acerta no veio mais próximo que estiver na área do golpe
static void cav_golpe_nos_veios(Caverna* c, Player* p) {
    int melhor = cav_veio_no_alcance(c, p);
    if (melhor < 0) return;

    CavVeio* v = &c->veios[melhor];
    v->vida--;
    v->flash = 10;
    v->tremor = 10;

    if (v->vida <= 0) {
        v->ativo = 0;
        if (v->idx_solido >= 0) c->solidos.itens[v->idx_solido].x = -10000.0f;   // libera a passagem
        c->coletados++;

        for (int k = 0; k < CAV_MAX_POP; k++) {                                   // ícone subindo
            if (c->pop[k].t <= 0) {
                c->pop[k].x = v->x + v->w / 2.0f;
                c->pop[k].y = v->y + v->h / 2.0f;
                c->pop[k].t = 45;
                break;
            }
        }
    }
}

// Retorna: 0 = continua na caverna, 1 = jogador pisou na placa de saída, 2 = jogador morreu
static int cav_atualizar(Caverna* c, Player* p) {
    atualizar_player(p, c->slimes, CAV_NUM_SLIMES, &c->solidos, c->area);

    // O veio leva o golpe quando a picareta desce (início do 2º quadro da animação)
    if (p->atacando && p->usando_picareta && p->frame_ataque == 1 && p->contador_ataque == 1) {
        cav_golpe_nos_veios(c, p);
    }

    atualizar_slimes(c->slimes, CAV_NUM_SLIMES, p, &c->solidos, c->area);

    for (int i = 0; i < CAV_NUM_VEIOS; i++) {
        if (c->veios[i].flash > 0) c->veios[i].flash--;
        if (c->veios[i].tremor > 0) c->veios[i].tremor--;
    }
    for (int i = 0; i < CAV_MAX_POP; i++) {
        if (c->pop[i].t > 0) { c->pop[i].t--; c->pop[i].y -= 1.0f; }
    }

    if (p->vida <= 0) return 2;
    if (colisao_pes_rect(p, c->saida, p->x, p->y)) return 1;
    return 0;
}

static int cav_completa(const Caverna* c) {
    return c->coletados >= CAV_META_MINERIO;
}


// ------------------------------------------------------------
// Desenho
// ------------------------------------------------------------
static int cav_comparar(const void* a, const void* b) {
    const CavItem* x = (const CavItem*)a;
    const CavItem* y = (const CavItem*)b;
    if (x->base_y != y->base_y) return (x->base_y < y->base_y) ? -1 : 1;
    if (x->tipo != y->tipo) return (int)x->tipo - (int)y->tipo;
    return x->indice - y->indice;
}

// Contador de minério (sem fonte: ícone + um quadradinho por minério)
static void cav_desenhar_hud(const Caverna* c) {
    float largura = 70 + CAV_META_MINERIO * 22;
    al_draw_filled_rectangle(12, 12, 12 + largura, 62, al_map_rgba(20, 20, 20, 190));
    al_draw_rectangle(12, 12, 12 + largura, 62, al_map_rgb(0, 0, 0), 2.0f);

    al_draw_scaled_bitmap(c->minerio, 0, 0, 12, 12, 20, 20, 36, 36, 0);

    for (int i = 0; i < CAV_META_MINERIO; i++) {
        float x = 70 + i * 22;
        if (i < c->coletados) {
            al_draw_filled_rectangle(x, 28, x + 16, 44, al_map_rgb(236, 146, 74));
        }
        al_draw_rectangle(x, 28, x + 16, 44, al_map_rgb(200, 200, 200), 2.0f);
    }
}

static void cav_desenhar_veio(const Caverna* c, int i) {
    const CavVeio* v = &c->veios[i];
    float off = (v->tremor > 0) ? ((v->tremor % 4 < 2) ? -2.0f : 2.0f) : 0.0f;
    float x = v->x + off, y = v->y;

    al_draw_filled_ellipse(v->x + v->w / 2.0f, v->y + v->h * 0.90f, v->w * 0.40f, 7, al_map_rgba(0, 0, 0, 95));
    al_draw_scaled_bitmap(c->veio_bmp, 0, 0, 16, 16, x, y, v->w, v->h, 0);

    // rachaduras conforme leva golpes
    int dano = CAV_VIDA_VEIO - v->vida;
    ALLEGRO_COLOR rach = al_map_rgb(20, 16, 22);
    if (dano >= 1) {
        al_draw_line(x + v->w * 0.46f, y + v->h * 0.28f, x + v->w * 0.54f, y + v->h * 0.50f, rach, 2.0f);
        al_draw_line(x + v->w * 0.54f, y + v->h * 0.50f, x + v->w * 0.44f, y + v->h * 0.66f, rach, 2.0f);
    }
    if (dano >= 2) {
        al_draw_line(x + v->w * 0.54f, y + v->h * 0.50f, x + v->w * 0.74f, y + v->h * 0.58f, rach, 2.0f);
        al_draw_line(x + v->w * 0.30f, y + v->h * 0.40f, x + v->w * 0.40f, y + v->h * 0.56f, rach, 2.0f);
    }

    // brilho do ferro (pisca de leve)
    if ((((int)(al_get_time() * 3.0)) + i) % 3 == 0) {
        al_draw_filled_rectangle(x + v->w * 0.36f, y + v->h * 0.36f, x + v->w * 0.36f + 4, y + v->h * 0.36f + 4, al_map_rgb(255, 235, 200));
    }

    // clarão ao levar um golpe
    if (v->flash > 0) {
        ALLEGRO_STATE st;
        al_store_state(&st, ALLEGRO_STATE_BLENDER);
        al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_ONE);
        al_draw_tinted_scaled_bitmap(c->veio_bmp, al_map_rgba_f(1.0f, 0.8f, 0.5f, 0.07f * v->flash),
            0, 0, 16, 16, x, y, v->w, v->h, 0);
        al_restore_state(&st);
    }
}

static void cav_desenhar(Caverna* c, Player* p, ALLEGRO_BITMAP* personagem, ALLEGRO_BITMAP* slime_sprite) {
    al_draw_bitmap(c->fundo, 0, 0, 0);
    desenhar_placa(c->saida);   // placa de saída

    CavItem fila[CAV_MAX_DECOR + CAV_NUM_VEIOS + CAV_NUM_SLIMES + 1];
    int n = 0;

    for (int i = 0; i < c->n_decor; i++) {
        fila[n].base_y = c->decor[i].base_y; fila[n].tipo = CAV_ITEM_DECOR; fila[n].indice = i; n++;
    }
    for (int i = 0; i < CAV_NUM_VEIOS; i++) {
        if (!c->veios[i].ativo) continue;
        fila[n].base_y = c->veios[i].y + c->veios[i].h * 0.90f; fila[n].tipo = CAV_ITEM_VEIO; fila[n].indice = i; n++;
    }
    for (int i = 0; i < CAV_NUM_SLIMES; i++) {
        if (c->slimes[i].vida <= 0) continue;
        fila[n].base_y = c->slimes[i].y + c->slimes[i].hit_y + c->slimes[i].hit_h;
        fila[n].tipo = CAV_ITEM_SLIME; fila[n].indice = i; n++;
    }
    // Jogador. Durante o golpe de picareta ele fica na frente do veio que está acertando,
    // senão a pedra cobre a picareta (a base da pedra fica abaixo dos pés do jogador).
    float base_player = p->y + p->hit_y + p->hit_h;
    if (p->atacando && p->usando_picareta) {
        int alvo = cav_veio_no_alcance(c, p);
        if (alvo >= 0) {
            float base_veio = c->veios[alvo].y + c->veios[alvo].h * 0.90f;
            if (base_player <= base_veio) base_player = base_veio + 0.5f;
        }
    }
    fila[n].base_y = base_player; fila[n].tipo = CAV_ITEM_PLAYER; fila[n].indice = 0; n++;

    qsort(fila, n, sizeof(CavItem), cav_comparar);

    for (int k = 0; k < n; k++) {
        switch (fila[k].tipo) {
        case CAV_ITEM_DECOR: {
            const CavDecor* d = &c->decor[fila[k].indice];
            al_draw_scaled_bitmap(d->bmp, 0, 0, al_get_bitmap_width(d->bmp), al_get_bitmap_height(d->bmp),
                d->x, d->y, d->w, d->h, 0);
            break;
        }
        case CAV_ITEM_VEIO:   cav_desenhar_veio(c, fila[k].indice); break;
        case CAV_ITEM_SLIME:  desenhar_slime(&c->slimes[fila[k].indice], slime_sprite); break;
        case CAV_ITEM_PLAYER: desenhar_player(p, personagem); break;
        }
    }

    // Ícone do minério subindo quando é coletado
    for (int i = 0; i < CAV_MAX_POP; i++) {
        if (c->pop[i].t <= 0) continue;
        float a = c->pop[i].t / 45.0f;
        al_draw_tinted_scaled_bitmap(c->minerio, al_map_rgba_f(a, a, a, a),
            0, 0, 12, 12, c->pop[i].x - 18, c->pop[i].y - 18, 36, 36, 0);
    }

    // Barras de vida dos slimes por cima de tudo
    for (int i = 0; i < CAV_NUM_SLIMES; i++) {
        if (c->slimes[i].vida <= 0) continue;
        desenhar_barra_vida(c->slimes[i].x + (64 - LARGURA_BARRA_SLIME) / 2.0f, c->slimes[i].y + 10,
            LARGURA_BARRA_SLIME, ALTURA_BARRA_SLIME, c->slimes[i].vida, VIDA_MAXIMA_SLIME);
    }

    cav_desenhar_hud(c);
}














#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum { MENU_JOGAR = 0, MENU_SAIR = 1 } ResultadoMenu;

#define MENU_NUM_BOTOES 2
#define MENU_NUM_PART   22

typedef struct {
    float x, y, vel, fase;
    int esc;
    const char* simbolo;
} MenuParticula;

/* ---------- fonte de pixels 5x7 (cada linha = 5 bits) ---------- */
static const unsigned char menu_glifos[128][8] = {
    [' '] = { 0 },
    ['.'] = { 0,0,0,0,0,0x0C,0x0C },
    [','] = { 0,0,0,0,0x0C,0x04,0x08 },
    ['!'] = { 0x04,0x04,0x04,0x04,0x04,0,0x04 },
    [':'] = { 0,0x0C,0x0C,0,0x0C,0x0C,0 },
    ['-'] = { 0,0,0,0x1F,0,0,0 },
    ['+'] = { 0,0x04,0x04,0x1F,0x04,0x04,0 },
    ['('] = { 0x02,0x04,0x08,0x08,0x08,0x04,0x02 },
    [')'] = { 0x08,0x04,0x02,0x02,0x02,0x04,0x08 },
    ['/'] = { 0x01,0x01,0x02,0x04,0x08,0x10,0x10 },

    ['0'] = { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E },
    ['1'] = { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E },
    ['2'] = { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F },
    ['3'] = { 0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E },
    ['4'] = { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 },
    ['5'] = { 0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E },
    ['6'] = { 0x06,0x08,0x10,0x1E,0x11,0x11,0x0E },
    ['7'] = { 0x1F,0x01,0x02,0x04,0x08,0x08,0x08 },
    ['8'] = { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E },
    ['9'] = { 0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C },

    ['A'] = { 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11 },
    ['B'] = { 0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E },
    ['C'] = { 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E },
    ['D'] = { 0x1E,0x11,0x11,0x11,0x11,0x11,0x1E },
    ['E'] = { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F },
    ['F'] = { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x10 },
    ['G'] = { 0x0E,0x11,0x10,0x17,0x11,0x11,0x0F },
    ['H'] = { 0x11,0x11,0x11,0x1F,0x11,0x11,0x11 },
    ['I'] = { 0x0E,0x04,0x04,0x04,0x04,0x04,0x0E },
    ['J'] = { 0x07,0x02,0x02,0x02,0x02,0x12,0x0C },
    ['K'] = { 0x11,0x12,0x14,0x18,0x14,0x12,0x11 },
    ['L'] = { 0x10,0x10,0x10,0x10,0x10,0x10,0x1F },
    ['M'] = { 0x11,0x1B,0x15,0x15,0x11,0x11,0x11 },
    ['N'] = { 0x11,0x19,0x15,0x13,0x11,0x11,0x11 },
    ['O'] = { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E },
    ['P'] = { 0x1E,0x11,0x11,0x1E,0x10,0x10,0x10 },
    ['Q'] = { 0x0E,0x11,0x11,0x11,0x15,0x12,0x0D },
    ['R'] = { 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11 },
    ['S'] = { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E },
    ['T'] = { 0x1F,0x04,0x04,0x04,0x04,0x04,0x04 },
    ['U'] = { 0x11,0x11,0x11,0x11,0x11,0x11,0x0E },
    ['V'] = { 0x11,0x11,0x11,0x11,0x11,0x0A,0x04 },
    ['W'] = { 0x11,0x11,0x11,0x15,0x15,0x1B,0x11 },
    ['X'] = { 0x11,0x11,0x0A,0x04,0x0A,0x11,0x11 },
    ['Y'] = { 0x11,0x11,0x0A,0x04,0x04,0x04,0x04 },
    ['Z'] = { 0x1F,0x01,0x02,0x04,0x08,0x10,0x1F },

    ['a'] = { 0,0,0x0E,0x01,0x0F,0x11,0x0F },
    ['b'] = { 0x10,0x10,0x16,0x19,0x11,0x11,0x1E },
    ['c'] = { 0,0,0x0E,0x10,0x10,0x11,0x0E },
    ['d'] = { 0x01,0x01,0x0D,0x13,0x11,0x11,0x0F },
    ['e'] = { 0,0,0x0E,0x11,0x1F,0x10,0x0E },
    ['f'] = { 0x06,0x09,0x08,0x1C,0x08,0x08,0x08 },
    ['g'] = { 0,0,0x0F,0x11,0x11,0x0F,0x01,0x0E },
    ['h'] = { 0x10,0x10,0x16,0x19,0x11,0x11,0x11 },
    ['i'] = { 0x04,0,0x0C,0x04,0x04,0x04,0x0E },
    ['j'] = { 0x02,0,0x06,0x02,0x02,0x02,0x12,0x0C },
    ['k'] = { 0x10,0x10,0x12,0x14,0x18,0x14,0x12 },
    ['l'] = { 0x0C,0x04,0x04,0x04,0x04,0x04,0x0E },
    ['m'] = { 0,0,0x1A,0x15,0x15,0x15,0x15 },
    ['n'] = { 0,0,0x16,0x19,0x11,0x11,0x11 },
    ['o'] = { 0,0,0x0E,0x11,0x11,0x11,0x0E },
    ['p'] = { 0,0,0x1E,0x11,0x11,0x1E,0x10,0x10 },
    ['q'] = { 0,0,0x0F,0x11,0x11,0x0F,0x01,0x01 },
    ['r'] = { 0,0,0x16,0x19,0x10,0x10,0x10 },
    ['s'] = { 0,0,0x0F,0x10,0x0E,0x01,0x1E },
    ['t'] = { 0x08,0x08,0x1C,0x08,0x08,0x09,0x06 },
    ['u'] = { 0,0,0x11,0x11,0x11,0x13,0x0D },
    ['v'] = { 0,0,0x11,0x11,0x11,0x0A,0x04 },
    ['w'] = { 0,0,0x11,0x11,0x15,0x15,0x0A },
    ['x'] = { 0,0,0x11,0x0A,0x04,0x0A,0x11 },
    ['y'] = { 0,0,0x11,0x11,0x11,0x0F,0x01,0x0E },
    ['z'] = { 0,0,0x1F,0x02,0x04,0x08,0x1F }
};

static float menu_rand(float a, float b)
{
    return a + (b - a) * ((float)rand() / (float)RAND_MAX);
}

static int menu_largura(const char* t, int esc)
{
    int n = (int)strlen(t);
    return (n > 0) ? n * 6 * esc - esc : 0;
}

/* maior escala (<= esc_max) em que o texto cabe em larg_max pixels */
static int menu_fit(const char* t, float larg_max, int esc_max)
{
    int e = esc_max;
    while (e > 1 && menu_largura(t, e) > larg_max) e--;
    return e;
}

/* esc = tamanho de cada "pixel" da fonte; centro=1 centraliza em x */
static void menu_texto(ALLEGRO_COLOR cor, float x, float y, int esc,
    int centro, const char* t)
{
    const unsigned char* p;
    float cx = centro ? x - menu_largura(t, esc) / 2.0f : x;

    for (p = (const unsigned char*)t; *p; p++) {
        if (*p < 128) {
            const unsigned char* g = menu_glifos[*p];
            int r, c;
            for (r = 0; r < 8; r++) {
                for (c = 0; c < 5; c++) {
                    if (g[r] & (0x10 >> c)) {
                        al_draw_filled_rectangle(cx + c * esc, y + r * esc,
                            cx + (c + 1) * esc,
                            y + (r + 1) * esc, cor);
                    }
                }
            }
        }
        cx += 6 * esc;
    }
}

/* Posicao/tamanho do botao i */
static void menu_botao_rect(int i, int w, int h,
    float* x, float* y, float* bw, float* bh)
{
    *bw = 320;
    *bh = 56;
    *x = (w - *bw) / 2.0f;
    *y = h * 0.62f + i * (*bh + 18);
}

static void menu_desenhar(MenuParticula* p, int sel, float t, int w, int h)
{
    static const char* rotulos[MENU_NUM_BOTOES] = { "JOGAR", "SAIR" };
    static const char* desc[3] = {
        "Um jogo educativo criado no PI do 2o semestre para ensinar",
        "sobre os elementos quimicos e suas curiosidades.",
        "Explore, minere e descubra os segredos da tabela periodica!"
    };
    static const char* curiosidades[3] = {
        "Curiosidade: o ferro (Fe) e a base do aco, o metal mais usado no mundo.",
        "Curiosidade: o ouro (Au) nao enferruja, pois e pouco reativo.",
        "Curiosidade: o hidrogenio (H) e o elemento mais abundante do universo."
    };
    const char* titulo = "ELEMENTAL ADVENTURE";
    const char* subtitulo = "Jogo educativo de Quimica";
    int i, e;
    float ty;

    /* fundo em degrade */
    for (i = 0; i < h; i += 4) {
        float k = (float)i / (float)h;
        al_draw_filled_rectangle(0, (float)i, (float)w, (float)i + 4,
            al_map_rgb(10 + (int)(20 * k), 14 + (int)(30 * k), 40 + (int)(50 * k)));
    }

    /* simbolos de elementos flutuando */
    for (i = 0; i < MENU_NUM_PART; i++) {
        float a = 0.15f + 0.15f * sinf(t * 1.5f + p[i].fase);
        menu_texto(al_map_rgba_f(0.4f * a, 0.8f * a, 1.0f * a, a),
            p[i].x, p[i].y, p[i].esc, 1, p[i].simbolo);
    }

    /* titulo */
    e = menu_fit(titulo, w * 0.9f, 6);
    ty = h * 0.10f + 4.0f * sinf(t * 2.0f);
    menu_texto(al_map_rgb(0, 0, 0), w / 2.0f + e, ty + e, e, 1, titulo);
    menu_texto(al_map_rgb(255, 205, 70), w / 2.0f, ty, e, 1, titulo);

    e = menu_fit(subtitulo, w * 0.9f, 3);
    menu_texto(al_map_rgb(140, 210, 255), w / 2.0f, h * 0.10f + 8 * 6 + 24, e, 1, subtitulo);

    /* descricao */
    e = menu_fit(desc[2], w * 0.92f, 2);
    for (i = 0; i < 3; i++) {
        menu_texto(al_map_rgb(225, 235, 250), w / 2.0f,
            h * 0.38f + i * (8 * e + 14), e, 1, desc[i]);
    }

    /* botoes */
    for (i = 0; i < MENU_NUM_BOTOES; i++) {
        float bx, by, bw, bh;
        int selecionado = (i == sel);
        menu_botao_rect(i, w, h, &bx, &by, &bw, &bh);

        al_draw_filled_rounded_rectangle(bx, by, bx + bw, by + bh, 12, 12,
            selecionado ? al_map_rgb(255, 190, 60) : al_map_rgb(40, 60, 110));
        al_draw_rounded_rectangle(bx, by, bx + bw, by + bh, 12, 12,
            selecionado ? al_map_rgb(255, 245, 200) : al_map_rgb(120, 160, 230),
            3);
        menu_texto(selecionado ? al_map_rgb(30, 25, 10) : al_map_rgb(255, 255, 255),
            bx + bw / 2.0f, by + (bh - 7 * 3) / 2.0f, 3, 1, rotulos[i]);
    }

    /* dica de controles e curiosidade rotativa */


    al_flip_display();
}

/* Roda o menu e devolve MENU_JOGAR ou MENU_SAIR */
ResultadoMenu tela_inicial(ALLEGRO_DISPLAY* display)
{
    static const char* simbolos[MENU_NUM_PART] = {
        "H", "He", "C", "N", "O", "Fe", "Au", "Na", "Cl", "Ca", "Si",
        "Cu", "Ag", "Zn", "S", "Al", "Li", "Ne", "K", "Mg", "P", "Pb"
    };
    MenuParticula part[MENU_NUM_PART];
    ALLEGRO_EVENT_QUEUE* fila;
    ALLEGRO_TIMER* timer;
    ResultadoMenu res = MENU_SAIR;
    int w = al_get_display_width(display);
    int h = al_get_display_height(display);
    int sel = 0, fim = 0, redesenhar = 1, i;
    float t = 0.0f;

    al_init_primitives_addon();
    if (!al_is_keyboard_installed()) al_install_keyboard();
    if (!al_is_mouse_installed())    al_install_mouse();

    timer = al_create_timer(1.0 / 60.0);
    fila = al_create_event_queue();
    if (!timer || !fila) {
        if (timer) al_destroy_timer(timer);
        if (fila)  al_destroy_event_queue(fila);
        return MENU_JOGAR; /* se falhar, segue direto para o jogo */
    }

    al_register_event_source(fila, al_get_display_event_source(display));
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_mouse_event_source());
    al_register_event_source(fila, al_get_timer_event_source(timer));

    for (i = 0; i < MENU_NUM_PART; i++) {
        part[i].x = menu_rand(0, (float)w);
        part[i].y = menu_rand(0, (float)h);
        part[i].vel = menu_rand(15, 45);
        part[i].esc = 2 + rand() % 3;      /* 2, 3 ou 4 */
        part[i].fase = menu_rand(0, 6.28f);
        part[i].simbolo = simbolos[i];
    }

    al_start_timer(timer);

    while (!fim) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(fila, &ev);

        switch (ev.type) {
        case ALLEGRO_EVENT_TIMER:
            t += 1.0f / 60.0f;
            for (i = 0; i < MENU_NUM_PART; i++) {
                part[i].y -= part[i].vel / 60.0f;
                if (part[i].y < -40) {
                    part[i].y = (float)h + 30;
                    part[i].x = menu_rand(0, (float)w);
                }
            }
            redesenhar = 1;
            break;

        case ALLEGRO_EVENT_DISPLAY_CLOSE:
            res = MENU_SAIR;
            fim = 1;
            break;

        case ALLEGRO_EVENT_KEY_DOWN:
            if (ev.keyboard.keycode == ALLEGRO_KEY_UP ||
                ev.keyboard.keycode == ALLEGRO_KEY_W) {
                sel = (sel + MENU_NUM_BOTOES - 1) % MENU_NUM_BOTOES;
            }
            else if (ev.keyboard.keycode == ALLEGRO_KEY_DOWN ||
                ev.keyboard.keycode == ALLEGRO_KEY_S) {
                sel = (sel + 1) % MENU_NUM_BOTOES;
            }
            else if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER ||
                ev.keyboard.keycode == ALLEGRO_KEY_PAD_ENTER ||
                ev.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                res = (sel == 0) ? MENU_JOGAR : MENU_SAIR;
                fim = 1;
            }
            else if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                res = MENU_SAIR;
                fim = 1;
            }
            break;

        case ALLEGRO_EVENT_MOUSE_AXES:
        case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
            for (i = 0; i < MENU_NUM_BOTOES; i++) {
                float bx, by, bw, bh;
                menu_botao_rect(i, w, h, &bx, &by, &bw, &bh);
                if (ev.mouse.x >= bx && ev.mouse.x <= bx + bw &&
                    ev.mouse.y >= by && ev.mouse.y <= by + bh) {
                    sel = i;
                    if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN &&
                        ev.mouse.button == 1) {
                        res = (i == 0) ? MENU_JOGAR : MENU_SAIR;
                        fim = 1;
                    }
                }
            }
            break;
        }

        if (!fim && redesenhar && al_is_event_queue_empty(fila)) {
            menu_desenhar(part, sel, t, w, h);
            redesenhar = 0;
        }
    }

    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    return res;
}

/* ======================================================================
 *  Atalho pronto: cria uma janela propria so para o menu, mostra a tela
 *  inicial e fecha a janela. Funciona sem mexer no resto do seu codigo.
 *  Chame na PRIMEIRA linha da main():
 *
 *        if (menu_inicial_executar() == MENU_SAIR) return 0;
 * ====================================================================== */
ResultadoMenu menu_inicial_executar(void)
{
    ResultadoMenu res;
    ALLEGRO_DISPLAY* janela;

    al_init();
    janela = al_create_display(960, 540);   /* troque o tamanho se quiser */
    if (!janela) return MENU_JOGAR;         /* se falhar, segue pro jogo  */
    al_set_window_title(janela, "Elemental Adventure");

    res = tela_inicial(janela);

    al_destroy_display(janela);
    return res;
}
























// ============================================================
// FUNÇÃO PRINCIPAL (MAIN)
// ============================================================

int main() {
    if (menu_inicial_executar() == MENU_SAIR) return 0;



    ALLEGRO_DISPLAY* janela = NULL;
    ALLEGRO_EVENT_QUEUE* fila = NULL;
    ALLEGRO_TIMER* timer = NULL;

    // Bitmaps do exterior
    ALLEGRO_BITMAP* personagem = NULL;
    ALLEGRO_BITMAP* personagem_acoes = NULL;
    ALLEGRO_BITMAP* grama = NULL;
    ALLEGRO_BITMAP* caminho = NULL;
    ALLEGRO_BITMAP* agua = NULL;
    ALLEGRO_BITMAP* slime_sprite = NULL;
    ALLEGRO_BITMAP* arvore = NULL;
    ALLEGRO_BITMAP* arvore_pequena = NULL;
    ALLEGRO_BITMAP* casa = NULL;
    ALLEGRO_BITMAP* bau = NULL;
    ALLEGRO_BITMAP* decor = NULL;
    ALLEGRO_BITMAP* cerca = NULL;
    ALLEGRO_BITMAP* vaca = NULL;
    ALLEGRO_BITMAP* porco = NULL;
    ALLEGRO_BITMAP* ovelha = NULL;
    ALLEGRO_BITMAP* galinha = NULL;
    ALLEGRO_BITMAP* fundo_ext = NULL;   // chão do exterior pré-desenhado uma vez só

    // Bitmaps do interior
    ALLEGRO_BITMAP* int_piso = NULL;
    ALLEGRO_BITMAP* int_parede_topo = NULL;
    ALLEGRO_BITMAP* int_parede_base = NULL;
    ALLEGRO_BITMAP* int_capacho = NULL;
    ALLEGRO_BITMAP* int_cama = NULL;
    ALLEGRO_BITMAP* int_mesa = NULL;
    ALLEGRO_BITMAP* int_escrivaninha = NULL;
    ALLEGRO_BITMAP* int_tapete = NULL;
    ALLEGRO_BITMAP* int_fornalha = NULL;
    ALLEGRO_BITMAP* int_estante = NULL;
    ALLEGRO_BITMAP* int_janela = NULL;
    ALLEGRO_BITMAP* int_bau = NULL;

    ALLEGRO_EVENT evento;
    int sair = 0;
    int slot_selecionado = 0;

    // Posições estáticas do cenário
    Posicao2D arvores[NUM_ARVORES] = { { 0, 0 },{ 1100, 20 },{ 1140, 540 },{ 20, 560 } };
    Posicao2D arvores_pequenas[NUM_ARVORES_PEQ] = { { 300, 100 },{ 500, 150 },{ 1000, 560 } };

    // Entidades
    Player player;
    Animal animais[NUM_ANIMAIS];

    // Cena atual e dados da casa
    Cena cena = CENA_EXTERIOR;
    Cena cena_origem = CENA_EXTERIOR;   // de onde o jogador veio na última troca de cena
    float casa_x = 40, casa_y = 60, casa_w = 0, casa_h = 0;

    Retangulo tela_ext = { 0, 0, LARGURA_TELA, ALTURA_TELA };
    // Área permitida para a HITBOX do jogador dentro da casa (começa acima do piso
    // porque só os pés precisam ficar no chão; a cabeça pode sobrepor a parede)
    Retangulo sala = { SALA_X, 68, SALA_LARGURA, SALA_BASE_Y - 68 };
    Retangulo parede_casa, porta_ext;
    Retangulo paredes[2];            // paredes sólidas dos lados da porta (para os pés)
    ListaSolidos solidos_ext = { 0 }; // tudo que bloqueia a passagem no exterior
    ListaSolidos solidos_int = { 0 }; // tudo que bloqueia a passagem no interior
    ListaCenario cenario = { 0 };     // sprites fixos do exterior (casa, árvores, postes...)
    Retangulo porta_int = { 608, 572, 64, 28 };   // capacho de saída

    // Fase 2: entrada da caverna (placa na ponta esquerda do caminho)
    Retangulo portal_ext = { 0, CAMINHO_Y, 40, CAMINHO_LINHAS * TILE };
    Caverna caverna = { 0 };

    // Móveis do interior
    Movel moveis[NUM_MOVEIS];
    int n_moveis = 0;

    // Transição (fade) entre cenas
    // fade_estado: 0 = normal, 1 = escurecendo, 2 = clareando
    int fade_estado = 0;
    float fade_alpha = 0.0f;
    Cena cena_destino = CENA_EXTERIOR;
    const float VELOCIDADE_FADE = 0.06f;

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
    if (!janela) {
        printf("ERRO: nao foi possivel criar a janela.\n");
        return -1;
    }

    timer = al_create_timer(1.0 / 60.0);
    fila = al_create_event_queue();

    // Carregamento de Sprites do exterior
    personagem = al_load_bitmap("Assets/Player.png");
    personagem_acoes = al_load_bitmap("Assets/Player_Actions.png");
    grama = al_load_bitmap("Assets/Grass_Middle.png");
    caminho = al_load_bitmap("Assets/Path_Tile.png");
    agua = al_load_bitmap("Assets/Water_Tile.png");
    slime_sprite = al_load_bitmap("Assets/Slime_Green.png");
    arvore = al_load_bitmap("Assets/Oak_Tree.png");
    arvore_pequena = al_load_bitmap("Assets/Oak_Tree_Small.png");
    casa = al_load_bitmap("Assets/House_1_Wood_Base_Blue.png");
    bau = al_load_bitmap("Assets/Chest.png");
    decor = al_load_bitmap("Assets/Outdoor_Decor_Free.png");
    cerca = al_load_bitmap("Assets/Fences.png");
    vaca = al_load_bitmap("Assets/Cow.png");
    porco = al_load_bitmap("Assets/Pig.png");
    ovelha = al_load_bitmap("Assets/Sheep.png");
    galinha = al_load_bitmap("Assets/Chicken.png");

    // Carregamento de Sprites do interior
    int_piso = al_load_bitmap("Assets/Interior_Floor.png");
    int_parede_topo = al_load_bitmap("Assets/Interior_Wall_Top.png");
    int_parede_base = al_load_bitmap("Assets/Interior_Wall_Base.png");
    int_capacho = al_load_bitmap("Assets/Interior_Door_Mat.png");
    int_cama = al_load_bitmap("Assets/Interior_Bed.png");
    int_mesa = al_load_bitmap("Assets/Interior_Table.png");
    int_escrivaninha = al_load_bitmap("Assets/Interior_Desk.png");
    int_tapete = al_load_bitmap("Assets/Interior_Rug.png");
    int_fornalha = al_load_bitmap("Assets/Interior_Furnace.png");
    int_estante = al_load_bitmap("Assets/Interior_Bookshelf.png");
    int_janela = al_load_bitmap("Assets/Interior_Window.png");
    int_bau = al_load_bitmap("Assets/Interior_Chest.png");

    if (!personagem || !personagem_acoes || !grama || !caminho || !agua || !slime_sprite ||
        !arvore || !arvore_pequena || !casa || !bau ||
        !decor || !cerca || !vaca || !porco || !ovelha || !galinha) {
        printf("ERRO: Nao foi possivel carregar um ou mais arquivos da pasta Assets.\n");
        return -1;
    }

    if (!int_piso || !int_parede_topo || !int_parede_base || !int_capacho || !int_cama ||
        !int_mesa || !int_escrivaninha || !int_tapete || !int_fornalha || !int_estante ||
        !int_janela || !int_bau) {
        printf("ERRO: Nao foi possivel carregar os assets do interior (Assets/Interior_*.png).\n");
        return -1;
    }

    // ========================================================
    // GEOMETRIA DA CASA (exterior)
    // ========================================================

    casa_w = al_get_bitmap_width(casa) * ESCALA_CASA;
    casa_h = al_get_bitmap_height(casa) * ESCALA_CASA;

    float base_y = casa_y + casa_h - 10;

    // Corpo da casa (valores que você já ajustou)
    parede_casa.x = casa_x + 40;
    parede_casa.w = 210;
    parede_casa.y = base_y - 369;
    parede_casa.h = 335;

    float linha_chao = parede_casa.y + parede_casa.h;   // "pé" da casa

    // Gatilho da porta: dentro do vão, um pouco acima do pé da casa
    porta_ext.x = casa_x + 100;
    porta_ext.w = 48;
    porta_ext.y = linha_chao - 26;
    porta_ext.h = 14;

    // Paredes sólidas para os PÉS do jogador: corpo da casa inteiro,
    // dividido em duas partes para deixar o vão da porta aberto
    paredes[0].x = parede_casa.x;
    paredes[0].y = parede_casa.y;
    paredes[0].w = porta_ext.x - parede_casa.x;
    paredes[0].h = parede_casa.h;

    paredes[1].x = porta_ext.x + porta_ext.w;
    paredes[1].y = parede_casa.y;
    paredes[1].w = (parede_casa.x + parede_casa.w) - paredes[1].x;
    paredes[1].h = parede_casa.h;

    // ========================================================
    // EXTERIOR: fundo pré-desenhado, obstáculos e animais
    // ========================================================
    fundo_ext = gerar_fundo_exterior(janela, grama, caminho, agua, decor);
    if (!fundo_ext) {
        printf("ERRO: Nao foi possivel criar o fundo do exterior.\n");
        return -1;
    }

    // Fase 2: carrega os assets da caverna (pasta assets_cave ao lado da pasta Assets)
    if (!cav_carregar(&caverna, janela)) {
        printf("ERRO: Nao foi possivel carregar a fase da caverna (pasta assets_cave).\n");
        return -1;
    }

    // Objetos sólidos: para um objeto novo ter colisão, basta adicioná-lo aqui.
    // A hitbox fica só na base do objeto, para o jogador poder passar atrás da parte de cima.

    // Paredes da casa
    adicionar_solido(&solidos_ext, paredes[0].x, paredes[0].y, paredes[0].w, paredes[0].h);
    adicionar_solido(&solidos_ext, paredes[1].x, paredes[1].y, paredes[1].w, paredes[1].h);

    // Lago (a margem faz parte do lago)
    adicionar_solido(&solidos_ext, LAGO_X + 18, LAGO_Y + 20, LAGO_COLS * TILE - 38, LAGO_LINHAS * TILE - 42);

    // Cerca do curral: topo, base, esquerda e direita
    adicionar_solido(&solidos_ext, CURRAL_X, CURRAL_Y + 14, CURRAL_COLS * TILE, 24);
    adicionar_solido(&solidos_ext, CURRAL_X, CURRAL_Y + (CURRAL_LINHAS - 1) * TILE + 14, CURRAL_COLS * TILE, 24);
    adicionar_solido(&solidos_ext, CURRAL_X + 14, CURRAL_Y, 20, CURRAL_LINHAS * TILE);
    adicionar_solido(&solidos_ext, CURRAL_X + (CURRAL_COLS - 1) * TILE + 14, CURRAL_Y, 20, CURRAL_LINHAS * TILE);

    // Árvores grandes (tronco)
    for (int i = 0; i < NUM_ARVORES; i++) {
        adicionar_solido(&solidos_ext, arvores[i].x + 52, arvores[i].y + 108, 24, 20);
    }

    // Árvores pequenas (tronco; a segunda árvore da folha é um pouco mais baixa)
    for (int i = 0; i < NUM_ARVORES_PEQ; i++) {
        float base = (i % 2) ? 87.0f : 96.0f;
        adicionar_solido(&solidos_ext, arvores_pequenas[i].x + 33, arvores_pequenas[i].y + base - 18, 18, 18);
    }

    // Postes de luz (base)
    adicionar_solido(&solidos_ext, POSTE_ESQ_X, POSTE_Y + 96, 27, 21);
    adicionar_solido(&solidos_ext, POSTE_DIR_X, POSTE_Y + 96, 27, 21);

    // Baú
    adicionar_solido(&solidos_ext, BAU_X, BAU_Y + 12, 32, 20);

    // Animais: vaca, porco e galinha no curral; ovelha solta na grama
    // (os 4 últimos números são a hitbox: hit_x, hit_y, hit_w, hit_h)
    animais[0] = (Animal){ vaca,    1040, 228, 0.4f,  1, 0, 0, 0,   0, 1034, 1160, 14, 34, 36, 20 };
    animais[1] = (Animal){ porco,   1110, 262, 0.6f, -1, 1, 7, 0,  40, 1034, 1160, 12, 28, 36, 18 };
    animais[2] = (Animal){ galinha, 1060, 296, 0.8f,  1, 0, 3, 0,  75, 1034, 1160, 22, 32, 20, 14 };
    animais[3] = (Animal){ ovelha,   200, 585, 0.5f,  1, 0, 9, 0,  20,  120,  420, 16, 28, 30, 18 };


    // Sprites fixos, desenhados em ordem de profundidade. A base da casa fica na altura da
    // porta: assim o jogador continua na frente da parede enquanto entra pelo vão.
    montar_cenario(&cenario, casa, arvore, arvore_pequena, bau, decor, cerca,
        arvores, arvores_pequenas, porta_ext.y);

    // Animais se movem: a hitbox acompanha a posição deles sozinha
    for (int i = 0; i < NUM_ANIMAIS; i++) {
        adicionar_solido_movel(&solidos_ext, &animais[i].x, &animais[i].y,
            animais[i].hit_x, animais[i].hit_y, animais[i].hit_w, animais[i].hit_h);
    }

    // ========================================================
    // MÓVEIS DO INTERIOR  (x, y já em pixels de tela)
    // ordem: o que é "plano" (fundo) primeiro
    // ========================================================
    adicionar_movel(moveis, &n_moveis, int_tapete, 590, 330, 1, 0);
    adicionar_movel(moveis, &n_moveis, int_capacho, 608, 568, 1, 0);
    adicionar_movel(moveis, &n_moveis, int_janela, 560, 62, 1, 0);
    adicionar_movel(moveis, &n_moveis, int_janela, 860, 62, 1, 0);
    adicionar_movel(moveis, &n_moveis, int_cama, 264, 106, 0, 1);
    adicionar_movel(moveis, &n_moveis, int_estante, 400, 90, 0, 1);
    adicionar_movel(moveis, &n_moveis, int_fornalha, 930, 60, 0, 1);
    adicionar_movel(moveis, &n_moveis, int_escrivaninha, 280, 330, 0, 1);
    adicionar_movel(moveis, &n_moveis, int_mesa, 612, 340, 0, 1);
    adicionar_movel(moveis, &n_moveis, int_bau, 975, 330, 0, 1);

    // Colisão dos móveis sólidos (só a metade de baixo, para os pés)
    for (int i = 0; i < n_moveis; i++) {
        if (!moveis[i].solido) continue;
        adicionar_solido(&solidos_int, moveis[i].x + 4, moveis[i].y + moveis[i].h * 0.5f,
            moveis[i].w - 8, moveis[i].h * 0.5f);
    }

    al_register_event_source(fila, al_get_display_event_source(janela));
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_timer_event_source(timer));

    al_start_timer(timer);

    // Inicializar Entidades
    init_player(&player);
    player.sprite_acoes = personagem_acoes;

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
                // Espada: acerta slimes, não acerta minério (igual em todas as cenas)
                if (!player.atacando) iniciar_golpe(&player, 0);
                break;
            case ALLEGRO_KEY_Q:
                // Picareta: funciona em todas as cenas, só quebra minério e não acerta slimes
                if (!player.atacando) {
                    iniciar_golpe(&player, 1);
                    if (cena == CENA_CAVERNA) cav_preparar_picareta(&caverna, &player);   // vira para o veio, se houver
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

            if (fade_estado == 0) {
                // Jogo normal
                if (cena == CENA_EXTERIOR) {
                    atualizar_player(&player, NULL, 0, &solidos_ext, tela_ext);   // primeira fase não tem slimes
                    atualizar_animais(animais, &player, &solidos_ext);

                    // Morreu: revive (mesma regra da caverna)
                    if (player.vida <= 0) {
                        reviver_player(&player);
                        cena_destino = CENA_EXTERIOR;
                        fade_estado = 1;
                    }
                    // Entrar na casa: basta pisar na porta
                    else if (colisao_pes_rect(&player, porta_ext, player.x, player.y)) {
                        cena_destino = CENA_INTERIOR;
                        fade_estado = 1;
                    }
                    // Entrar na caverna: basta pisar na placa à esquerda
                    else if (colisao_pes_rect(&player, portal_ext, player.x, player.y)) {
                        cena_destino = CENA_CAVERNA;
                        fade_estado = 1;
                    }
                }
                else if (cena == CENA_INTERIOR) {
                    atualizar_player(&player, NULL, 0, &solidos_int, sala);

                    // Sair da casa: basta pisar no capacho
                    if (colisao_pes_rect(&player, porta_int, player.x, player.y)) {
                        cena_destino = CENA_EXTERIOR;
                        fade_estado = 1;
                    }
                }
                else {
                    // CENA_CAVERNA: 0 = continua, 1 = pisou na saída, 2 = morreu
                    int r = cav_atualizar(&caverna, &player);
                    if (r == 1) {
                        cena_destino = CENA_EXTERIOR;
                        fade_estado = 1;
                    }
                    else if (r == 2) {
                        reviver_player(&player);
                        cav_resetar(&caverna);
                        cena_destino = CENA_EXTERIOR;
                        fade_estado = 1;
                    }
                }
            }
            else if (fade_estado == 1) {
                // Escurecendo; quando fica tudo preto, troca a cena
                fade_alpha += VELOCIDADE_FADE;
                if (fade_alpha >= 1.0f) {
                    fade_alpha = 1.0f;
                    cena_origem = cena;
                    cena = cena_destino;

                    if (cena == CENA_INTERIOR) {
                        player.x = 640 - (player.hit_x + player.hit_w / 2);
                        player.y = 470;
                        player.ultima_direcao = 2;
                    }
                    else if (cena == CENA_CAVERNA) {
                        // aparece perto da placa de saída, virado para a direita
                        player.x = 100;
                        player.y = 330;
                        player.ultima_direcao = 1;
                    }
                    else if (cena_origem == CENA_CAVERNA) {
                        // voltando da caverna: aparece na ponta esquerda do caminho
                        player.x = 60;
                        player.y = CAMINHO_Y + 40;
                        player.ultima_direcao = 1;
                    }
                    else {
                        player.x = porta_ext.x + porta_ext.w / 2 - (player.hit_x + player.hit_w / 2);
                        player.y = linha_chao + 14 - (player.hit_y + player.hit_h);
                        player.ultima_direcao = 0;
                    }
                    fade_estado = 2;
                }
            }
            else {
                // Clareando
                fade_alpha -= VELOCIDADE_FADE;
                if (fade_alpha <= 0.0f) {
                    fade_alpha = 0.0f;
                    fade_estado = 0;
                }
            }

            // --- RENDERIZAÇÃO (DRAW) ---
            al_clear_to_color(al_map_rgb(0, 0, 0));

            float pes_y = player.y + player.hit_y + player.hit_h;

            if (cena == CENA_EXTERIOR) {
                // 1. Chão
                al_draw_bitmap(fundo_ext, 0, 0, 0);

                // Placa: entrada da caverna
                desenhar_placa(portal_ext);

                // 2. Cenário, animais e jogador, em ordem de profundidade
                desenhar_exterior(&cenario, animais, &player, personagem);

                // DEBUG: descomente para ver as hitboxes
                // (vermelho = objetos sólidos, verde = gatilhos)
                /*
                for (int k = 0; k < solidos_ext.n; k++) {
                    Retangulo c = caixa_solido(&solidos_ext.itens[k]);
                    al_draw_rectangle(c.x, c.y, c.x + c.w, c.y + c.h, al_map_rgb(255, 0, 0), 2);
                }
                al_draw_rectangle(porta_ext.x, porta_ext.y, porta_ext.x + porta_ext.w, porta_ext.y + porta_ext.h, al_map_rgb(0, 255, 0), 2);
                al_draw_rectangle(portal_ext.x, portal_ext.y, portal_ext.x + portal_ext.w, portal_ext.y + portal_ext.h, al_map_rgb(0, 255, 0), 2);
                */
            }
            else if (cena == CENA_INTERIOR) {
                // Interior: piso e paredes, depois os móveis que ficam ATRÁS do jogador
                desenhar_interior_fundo(int_piso, int_parede_topo, int_parede_base);
                desenhar_moveis(moveis, n_moveis, pes_y, 0);

                // Jogador e os móveis que ficam NA FRENTE dele
                desenhar_player(&player, personagem);
                desenhar_moveis(moveis, n_moveis, pes_y, 1);
            }
            else {
                // Caverna: fundo, objetos, slimes, minérios e jogador (ordenados) + contador
                cav_desenhar(&caverna, &player, personagem, slime_sprite);
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
            desenhar_hud(slot_selecionado, caverna.minerio, caverna.coletados);

            // 5. Fade (por cima de tudo)
            if (fade_alpha > 0.0f) {
                al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba_f(0, 0, 0, fade_alpha));
            }

            al_flip_display();
        }
    }

    // ========================================================
    // FINALIZAÇÃO DE RECURSOS
    // ========================================================

    cav_destruir(&caverna);

    al_destroy_bitmap(personagem);
    al_destroy_bitmap(personagem_acoes);
    al_destroy_bitmap(grama);
    al_destroy_bitmap(caminho);
    al_destroy_bitmap(agua);
    al_destroy_bitmap(slime_sprite);
    al_destroy_bitmap(arvore);
    al_destroy_bitmap(arvore_pequena);
    al_destroy_bitmap(casa);
    al_destroy_bitmap(bau);
    al_destroy_bitmap(decor);
    al_destroy_bitmap(cerca);
    al_destroy_bitmap(vaca);
    al_destroy_bitmap(porco);
    al_destroy_bitmap(ovelha);
    al_destroy_bitmap(galinha);
    al_destroy_bitmap(fundo_ext);

    al_destroy_bitmap(int_piso);
    al_destroy_bitmap(int_parede_topo);
    al_destroy_bitmap(int_parede_base);
    al_destroy_bitmap(int_capacho);
    al_destroy_bitmap(int_cama);
    al_destroy_bitmap(int_mesa);
    al_destroy_bitmap(int_escrivaninha);
    al_destroy_bitmap(int_tapete);
    al_destroy_bitmap(int_fornalha);
    al_destroy_bitmap(int_estante);
    al_destroy_bitmap(int_janela);
    al_destroy_bitmap(int_bau);

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

// Colisão usando só os PÉS do jogador (faixa de baixo da hitbox).
// Assim a cabeça pode "passar na frente" da casa, como nos jogos top-down.
int colisao_pes_rect(Player* p, Retangulo r, float novo_x, float novo_y) {
    float pes_h = 10;
    return colisao(
        novo_x + p->hit_x, novo_y + p->hit_y + p->hit_h - pes_h, p->hit_w, pes_h,
        r.x, r.y, r.w, r.h
    );
}

// Registra um objeto sólido parado (árvore, poste, móvel...), com a hitbox em pixels de tela
void adicionar_solido(ListaSolidos* lista, float x, float y, float w, float h) {
    if (lista->n >= MAX_SOLIDOS) {
        printf("AVISO: limite de objetos solidos atingido (aumente MAX_SOLIDOS).\n");
        return;
    }

    ObjetoSolido* s = &lista->itens[lista->n++];
    s->x = x;
    s->y = y;
    s->w = w;
    s->h = h;
    s->seg_x = NULL;
    s->seg_y = NULL;
}

// Registra um objeto sólido que se move: passe o endereço da posição dele e a hitbox relativa a ela
void adicionar_solido_movel(ListaSolidos* lista, const float* x, const float* y, float hit_x, float hit_y, float w, float h) {
    int antes = lista->n;

    adicionar_solido(lista, hit_x, hit_y, w, h);
    if (lista->n == antes) return;   // lista cheia

    lista->itens[antes].seg_x = x;
    lista->itens[antes].seg_y = y;
}

// Devolve a hitbox do objeto na posição atual dele
Retangulo caixa_solido(const ObjetoSolido* s) {
    Retangulo r = { s->x, s->y, s->w, s->h };
    if (s->seg_x) {
        r.x += *s->seg_x;
        r.y += *s->seg_y;
    }
    return r;
}

// Diz se o retângulo encosta em algum objeto sólido da lista.
// "ignorar" serve para um objeto móvel não colidir consigo mesmo (passe NULL se não precisar).
int ha_solido_em(const ListaSolidos* lista, float x, float y, float w, float h, const float* ignorar) {
    for (int i = 0; i < lista->n; i++) {
        if (ignorar && lista->itens[i].seg_x == ignorar) continue;

        Retangulo r = caixa_solido(&lista->itens[i]);
        if (colisao(x, y, w, h, r.x, r.y, r.w, r.h)) return 1;
    }
    return 0;
}

// Começa um golpe: picareta = 1 (tecla Q) ou espada = 0 (tecla E). Vale para todas as cenas.
void iniciar_golpe(Player* p, int picareta) {
    p->atacando = 1;
    p->frame_ataque = 0;
    p->contador_ataque = 0;
    p->ataque_acertou = 0;
    p->usando_picareta = picareta;
}

// Área que o golpe acerta, à frente do jogador. É a MESMA para a espada e para a picareta,
// em todas as cenas: mudar o alcance aqui muda as duas ferramentas.
#define ALCANCE_GOLPE 55.0f
Retangulo area_golpe(const Player* p) {
    Retangulo r = { p->x, p->y, (float)p->largura_scaled, (float)p->altura_scaled };

    if (p->ultima_direcao == 1) { r.x += p->largura_scaled - 10; r.w = ALCANCE_GOLPE; }
    else if (p->ultima_direcao == 3) { r.x -= ALCANCE_GOLPE - 10; r.w = ALCANCE_GOLPE; }
    else if (p->ultima_direcao == 2) { r.y -= ALCANCE_GOLPE - 10; r.h = ALCANCE_GOLPE; }
    else { r.y += p->altura_scaled - 10; r.h = ALCANCE_GOLPE; }
    return r;
}

// Devolve a vida do jogador depois de morrer (quem chama reinicia a fase e troca de cena)
void reviver_player(Player* p) {
    p->vida = VIDA_MAXIMA_PLAYER;
    p->invulneravel_timer = 0;
    p->atacando = 0;
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
    p->usando_picareta = 0;

    p->hit_x = 30;
    p->hit_y = 14;
    p->hit_w = 36;
    p->hit_h = 62;
}

void atualizar_player(Player* p, Slime slimes[], int n_slimes,
    const ListaSolidos* solidos, Retangulo area) {

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

        // Objetos sólidos (casa, árvores, animais, móveis...) — usam só os pés
        for (int j = 0; j < solidos->n; j++) {
            Retangulo caixa = caixa_solido(&solidos->itens[j]);
            if (colisao_pes_rect(p, caixa, novo_x, p->y)) pode_mover_x = 0;
            if (colisao_pes_rect(p, caixa, p->x, novo_y)) pode_mover_y = 0;
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

        // Só a espada acerta slimes; a picareta só quebra minério
        if (p->contador_ataque == 4 && !p->ataque_acertou && !p->usando_picareta) {
            Retangulo g = area_golpe(p);

            for (int i = 0; i < n_slimes; i++) {
                if (slimes[i].vida <= 0) continue;

                if (colisao(g.x, g.y, g.w, g.h, slimes[i].x + slimes[i].hit_x, slimes[i].y + slimes[i].hit_y, slimes[i].hit_w, slimes[i].hit_h)) {
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

// Diz se o slime (já na posição de teste) bateu no jogador, em objeto sólido ou na borda da área
static int slime_bateu(const Slime* t, Player* p, const ListaSolidos* solidos, Retangulo area) {
    float hx = t->x + t->hit_x, hy = t->y + t->hit_y;

    if (colisao_player_slime(p, (Slime*)t, p->x, p->y)) return 1;
    if (hx < area.x || hy < area.y || hx + t->hit_w > area.x + area.w || hy + t->hit_h > area.y + area.h) return 1;
    return ha_solido_em(solidos, hx, hy, t->hit_w, t->hit_h, NULL);
}

// Comportamento dos slimes, igual em todas as fases: patrulham, perseguem o jogador
// quando ele chega perto e tiram vida ao encostar
void atualizar_slimes(Slime slimes[], int n_slimes, Player* p, const ListaSolidos* solidos, Retangulo area) {
    for (int i = 0; i < n_slimes; i++) {
        Slime* s = &slimes[i];
        if (s->vida <= 0) continue;

        float pcx = p->x + p->hit_x + p->hit_w / 2.0f;
        float pcy = p->y + p->hit_y + p->hit_h / 2.0f;
        float scx = s->x + s->hit_x + s->hit_w / 2.0f;
        float scy = s->y + s->hit_y + s->hit_h / 2.0f;
        float dx = pcx - scx, dy = pcy - scy;
        float dist = sqrtf(dx * dx + dy * dy);

        // Perto do jogador: persegue. Longe: patrulha na horizontal.
        int perseguindo = (dist < RAIO_PERSEGUIR_SLIME && dist > 1.0f);
        float vx, vy;
        if (perseguindo) {
            float v = s->velocidade * 1.3f;
            vx = dx / dist * v;
            vy = dy / dist * v;
        }
        else {
            vx = s->velocidade * s->direcao;
            vy = 0;
        }

        // Eixo X
        Slime t = *s;
        t.x += vx;
        int bateu_x = slime_bateu(&t, p, solidos, area);
        if (bateu_x) { if (!perseguindo) s->direcao *= -1; }
        else s->x = t.x;

        // Eixo Y
        t = *s;
        t.y += vy;
        if (!slime_bateu(&t, p, solidos, area)) s->y = t.y;

        // Limites da patrulha: se saiu deles perseguindo o jogador, volta andando (sem teleportar).
        // Se acabou de bater em algo, mantém a meia-volta para não ficar empurrando o obstáculo.
        if (!perseguindo && !bateu_x) {
            if (s->x <= s->limite_esquerda) s->direcao = 1;
            if (s->x >= s->limite_direita) s->direcao = -1;
        }

        // Encostou no jogador: tira vida (com invulnerabilidade)
        if (p->invulneravel_timer == 0 && p->vida > 0 &&
            colisao(p->x + p->hit_x - 4, p->y + p->hit_y - 4, p->hit_w + 8, p->hit_h + 8,
                s->x + s->hit_x, s->y + s->hit_y, s->hit_w, s->hit_h)) {
            p->vida--;
            p->invulneravel_timer = TEMPO_INVULNERAVEL;
        }

        s->contador++;
        if (s->contador >= 10) {
            s->frame = (s->frame + 1) % 4;
            s->contador = 0;
        }
        if (s->dano_timer > 0) s->dano_timer--;
    }
}

void adicionar_movel(Movel lista[], int* n, ALLEGRO_BITMAP* bmp, float x, float y, int plano, int solido) {
    Movel* m = &lista[*n];
    m->bmp = bmp;
    m->x = x;
    m->y = y;
    m->w = al_get_bitmap_width(bmp) * ESCALA_MOVEIS;
    m->h = al_get_bitmap_height(bmp) * ESCALA_MOVEIS;
    m->plano = plano;
    m->solido = solido;
    (*n)++;
}

void desenhar_barra_vida(float x, float y, float largura, float altura, int vida_atual, int vida_maxima) {
    if (vida_atual <= 0) return;

    float pct = (float)vida_atual / vida_maxima;

    al_draw_filled_rectangle(x, y, x + largura, y + altura, al_map_rgb(100, 0, 0));
    al_draw_filled_rectangle(x, y, x + (largura * pct), y + altura, al_map_rgb(0, 230, 0));
    al_draw_rectangle(x, y, x + largura, y + altura, al_map_rgb(0, 0, 0), 1.0);
}

// ------------------------------------------------------------
// EXTERIOR
// ------------------------------------------------------------

// Desenha um pedaço (sx, sy, sw, sh) de uma folha de sprites com escala inteira
static void desenhar_regiao(ALLEGRO_BITMAP* bmp, float sx, float sy, float sw, float sh,
    float dx, float dy, float escala) {
    al_draw_scaled_bitmap(bmp, sx, sy, sw, sh, dx, dy, sw * escala, sh * escala, 0);
}

// Preenche um retângulo de (cols x linhas) tiles usando um tileset 3x3
// (cantos, bordas e miolo), como Water_Tile.png e Path_Tile.png
static void desenhar_patch(ALLEGRO_BITMAP* tileset, float x, float y, int cols, int linhas) {
    for (int r = 0; r < linhas; r++) {
        for (int c = 0; c < cols; c++) {
            int sx = (c == 0) ? 0 : (c == cols - 1 ? 2 : 1);
            int sy = (r == 0) ? 0 : (r == linhas - 1 ? 2 : 1);
            desenhar_regiao(tileset, sx * 16, sy * 16, 16, 16, x + c * TILE, y + r * TILE, ESCALA_TILE);
        }
    }
}

// Número "aleatório" fixo para cada célula do mapa (a decoração não muda a cada execução)
static unsigned hash_celula(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ 0x9e3779b1u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// Diz se uma célula está livre para receber decoração (longe de caminho, lago, casa, curral, etc.)
static int celula_livre(int cx, int cy) {
    int px = cx * TILE, py = cy * TILE;

    if (py + TILE > CAMINHO_Y - 8 && py < CAMINHO_Y + CAMINHO_LINHAS * TILE + 8) return 0;           // caminho
    if (px + TILE > LAGO_X - TILE && px < LAGO_X + (LAGO_COLS + 1) * TILE &&
        py + TILE > LAGO_Y - TILE && py < LAGO_Y + (LAGO_LINHAS + 1) * TILE) return 0;               // lago
    if (px + TILE > 30 && px < 350 && py + TILE > 50 && py < 460) return 0;                         // casa
    if (px + TILE > CURRAL_X - TILE && px < CURRAL_X + (CURRAL_COLS + 1) * TILE &&
        py + TILE > CURRAL_Y - TILE && py < CURRAL_Y + (CURRAL_LINHAS + 1) * TILE) return 0;         // curral
    if (px + TILE > 470 && px < 600 && py + TILE > 260 && py < 360) return 0;                       // baú
    if (px >= 840 && px < 930 && py >= 270 && py < 390) return 0;                                    // poste de luz
    if (px > 1000 && py < 180) return 0;                                                             // árvore do canto
    return 1;
}

// Desenha UMA vez só todo o chão fixo do exterior (grama, flores, caminho e lago)
// num bitmap grande. Assim o jogo só precisa copiar esse bitmap a cada frame.
ALLEGRO_BITMAP* gerar_fundo_exterior(ALLEGRO_DISPLAY* janela, ALLEGRO_BITMAP* grama, ALLEGRO_BITMAP* caminho,
    ALLEGRO_BITMAP* agua, ALLEGRO_BITMAP* decor) {

    ALLEGRO_BITMAP* fundo = al_create_bitmap(LARGURA_TELA, ALTURA_TELA);
    if (!fundo) return NULL;

    al_set_target_bitmap(fundo);

    // Grama
    for (int yy = 0; yy < ALTURA_TELA; yy += TILE) {
        for (int xx = 0; xx < LARGURA_TELA; xx += TILE) {
            desenhar_regiao(grama, 0, 0, 16, 16, xx, yy, ESCALA_TILE);
        }
    }

    // Decoração espalhada na grama (tufos, flores, cogumelos, tulipas e pedrinhas)
    for (int cy = 0; cy < ALTURA_TELA / TILE; cy++) {
        for (int cx = 0; cx < LARGURA_TELA / TILE + 1; cx++) {
            if (!celula_livre(cx, cy)) continue;

            unsigned v = hash_celula(cx, cy);
            int sorteio = v % 100;
            int variacao = (v >> 8) % 3;
            float dx = cx * TILE, dy = cy * TILE;

            if (sorteio < 10)       desenhar_regiao(decor, variacao * 16, 0, 16, 16, dx, dy, ESCALA_TILE);      // tufos
            else if (sorteio < 14)  desenhar_regiao(decor, variacao * 16, 16, 16, 16, dx, dy, ESCALA_TILE);     // flores
            else if (sorteio < 16)  desenhar_regiao(decor, 32, 112, 16, 16, dx, dy, ESCALA_TILE);               // cogumelo
            else if (sorteio < 18)  desenhar_regiao(decor, (variacao % 2) * 16, 128, 16, 16, dx, dy, ESCALA_TILE); // tulipas
            else if (sorteio < 20)  desenhar_regiao(decor, 16 + (variacao % 2) * 16, 32, 16, 16, dx, dy, ESCALA_TILE); // pedras
        }
    }

    // Caminho (começa fora da tela para as pontas não ficarem arredondadas)
    desenhar_patch(caminho, -TILE, CAMINHO_Y, LARGURA_TELA / TILE + 3, CAMINHO_LINHAS);

    // Lago e ondinhas na água
    desenhar_patch(agua, LAGO_X, LAGO_Y, LAGO_COLS, LAGO_LINHAS);
    desenhar_regiao(agua, 0, 80, 16, 16, LAGO_X + 2 * TILE, LAGO_Y + 1 * TILE, ESCALA_TILE);
    desenhar_regiao(agua, 16, 80, 16, 16, LAGO_X + 4 * TILE, LAGO_Y + 2 * TILE, ESCALA_TILE);
    desenhar_regiao(agua, 32, 80, 16, 16, LAGO_X + 3 * TILE, LAGO_Y + 2 * TILE, ESCALA_TILE);
    desenhar_regiao(agua, 16, 80, 16, 16, LAGO_X + 4 * TILE, LAGO_Y + 1 * TILE, ESCALA_TILE);

    // Volta a desenhar na tela
    al_set_target_backbuffer(janela);
    return fundo;
}

// Registra um sprite fixo do cenário: para um objeto novo aparecer na ordem certa, basta adicioná-lo aqui
void adicionar_sprite(ListaCenario* lista, ALLEGRO_BITMAP* bmp, float sx, float sy, float sw, float sh,
    float dx, float dy, float escala, float base_y) {

    if (lista->n >= MAX_SPRITES) {
        printf("AVISO: limite de sprites do cenario atingido (aumente MAX_SPRITES).\n");
        return;
    }

    lista->itens[lista->n++] = (SpriteCenario){ bmp, sx, sy, sw, sh, dx, dy, escala, base_y };
}

// Monta a lista de sprites fixos do exterior. Cada base_y é o fim da hitbox do objeto.
void montar_cenario(ListaCenario* lista, ALLEGRO_BITMAP* casa, ALLEGRO_BITMAP* arvore,
    ALLEGRO_BITMAP* arvore_peq, ALLEGRO_BITMAP* bau, ALLEGRO_BITMAP* decor, ALLEGRO_BITMAP* cerca,
    Posicao2D arvores[], Posicao2D arvores_peq[], float base_casa) {

    // Casa
    adicionar_sprite(lista, casa, 0, 0, al_get_bitmap_width(casa), al_get_bitmap_height(casa),
        40, 60, ESCALA_CASA, base_casa);

    // Cerca do curral: linhas de cima e de baixo (peças esquerda / meio / direita)...
    float base_cerca = 38;   // distância do topo da peça até o fim da hitbox da cerca
    float y_baixo = CURRAL_Y + (CURRAL_LINHAS - 1) * TILE;
    for (int i = 0; i < CURRAL_COLS; i++) {
        int col = (i == 0) ? 1 : (i == CURRAL_COLS - 1 ? 3 : 2);
        float x = CURRAL_X + i * TILE;
        adicionar_sprite(lista, cerca, col * 16, 0, 16, 16, x, CURRAL_Y, ESCALA_TILE, CURRAL_Y + base_cerca);
        adicionar_sprite(lista, cerca, col * 16, 0, 16, 16, x, y_baixo, ESCALA_TILE, y_baixo + base_cerca);
    }
    // ...e as laterais (peças verticais)
    for (int r = 1; r < CURRAL_LINHAS - 1; r++) {
        float y = CURRAL_Y + r * TILE;
        adicionar_sprite(lista, cerca, 0, 16, 16, 16, CURRAL_X, y, ESCALA_TILE, y + base_cerca);
        adicionar_sprite(lista, cerca, 0, 16, 16, 16, CURRAL_X + (CURRAL_COLS - 1) * TILE, y, ESCALA_TILE, y + base_cerca);
    }

    // Postes de luz ao longo do caminho
    adicionar_sprite(lista, decor, 68, 72, 10, 40, POSTE_ESQ_X, POSTE_Y, ESCALA_TILE, POSTE_Y + 117);
    adicionar_sprite(lista, decor, 68, 72, 10, 40, POSTE_DIR_X, POSTE_Y, ESCALA_TILE, POSTE_Y + 117);

    // Árvores Grandes
    int sw = al_get_bitmap_width(arvore), sh = al_get_bitmap_height(arvore);
    for (int i = 0; i < NUM_ARVORES; i++) {
        adicionar_sprite(lista, arvore, 0, 0, sw, sh, arvores[i].x, arvores[i].y, ESCALA_ARVORE, arvores[i].y + 128);
    }

    // Árvores Pequenas (a folha tem 2 árvores: alterna entre elas; a segunda é mais baixa)
    for (int i = 0; i < NUM_ARVORES_PEQ; i++) {
        int sx = 34 + (i % 2) * 32;
        float base = (i % 2) ? 87.0f : 96.0f;
        adicionar_sprite(lista, arvore_peq, sx, 0, 28, 48, arvores_peq[i].x, arvores_peq[i].y, ESCALA_ARVORE_PEQ, arvores_peq[i].y + base);
    }

    // Baú
    adicionar_sprite(lista, bau, 0, 0, 16, 16, BAU_X, BAU_Y, 2, BAU_Y + 32);
}

// Ordem de desenho: de cima para baixo na tela. No empate, a ordem é sempre a mesma, para não piscar.
static int comparar_itens(const void* a, const void* b) {
    const ItemDesenho* x = (const ItemDesenho*)a;
    const ItemDesenho* y = (const ItemDesenho*)b;

    if (x->base_y != y->base_y) return (x->base_y < y->base_y) ? -1 : 1;
    if (x->tipo != y->tipo) return (int)x->tipo - (int)y->tipo;
    return x->indice - y->indice;
}

// Desenha tudo que fica em pé no exterior, de trás para a frente
void desenhar_exterior(const ListaCenario* cenario, Animal animais[], Player* p, ALLEGRO_BITMAP* personagem) {

    ItemDesenho fila[MAX_SPRITES + NUM_ANIMAIS + 1];
    int n = 0;

    for (int i = 0; i < cenario->n; i++) {
        fila[n++] = (ItemDesenho){ cenario->itens[i].base_y, ITEM_CENARIO, i };
    }
    for (int i = 0; i < NUM_ANIMAIS; i++) {
        fila[n++] = (ItemDesenho){ animais[i].y + animais[i].hit_y + animais[i].hit_h, ITEM_ANIMAL, i };
    }
    fila[n++] = (ItemDesenho){ p->y + p->hit_y + p->hit_h, ITEM_PLAYER, 0 };


    qsort(fila, n, sizeof(ItemDesenho), comparar_itens);

    for (int i = 0; i < n; i++) {
        switch (fila[i].tipo) {
        case ITEM_CENARIO: {
            const SpriteCenario* s = &cenario->itens[fila[i].indice];
            desenhar_regiao(s->bmp, s->sx, s->sy, s->sw, s->sh, s->dx, s->dy, s->escala);
            break;
        }
        case ITEM_ANIMAL: desenhar_animal(&animais[fila[i].indice]); break;
        case ITEM_PLAYER: desenhar_player(p, personagem); break;
        }
    }
}

void desenhar_slime(Slime* s, ALLEGRO_BITMAP* sprite) {
    if (s->dano_timer <= 0) {
        al_draw_bitmap_region(sprite, s->frame * 64, 0, 64, 64, s->x, s->y, 0);
    }
    else {
        al_draw_tinted_bitmap_region(
            sprite, al_map_rgba(255, 50, 50, 255),
            s->frame * 64, 0, 64, 64, s->x, s->y, 0
        );
    }
}

void desenhar_player(Player* p, ALLEGRO_BITMAP* sprite) {
    if (p->invulneravel_timer % 10 >= 5) return;   // pisca enquanto está invulnerável

    int flip = (p->ultima_direcao == 3) ? ALLEGRO_FLIP_HORIZONTAL : 0;   // as duas folhas olham para a direita
    int coluna = p->coluna, linha = p->linha;

    // Picareta: a folha tem quadros de 48x48 (Player.png usa 32x32) e cada linha tem 2 quadros,
    // levanta e bate. O personagem fica 8 px mais para dentro do quadro, por isso o ajuste.
    if (p->atacando && p->usando_picareta) {
        int linha_acao = (p->ultima_direcao == 0) ? 1 : (p->ultima_direcao == 2) ? 2 : 0;   // baixo, cima, lado
        float tam = 48.0f * p->escala;
        float ajuste = 8.0f * p->escala;

        al_draw_scaled_bitmap(p->sprite_acoes, p->frame_ataque * 48, linha_acao * 48, 48, 48,
            p->x - ajuste, p->y - ajuste, tam, tam, flip);
        return;
    }

    if (p->atacando) {
        coluna = p->frame_ataque;
        if (p->ultima_direcao == 1 || p->ultima_direcao == 3) linha = 7;
        else if (p->ultima_direcao == 2) linha = 8;
        else linha = 6;
    }

    al_draw_scaled_bitmap(
        sprite,
        coluna * p->largura, linha * p->altura, p->largura, p->altura,
        p->x, p->y, p->largura_scaled, p->altura_scaled, flip
    );
}

void atualizar_animais(Animal animais[], Player* p, const ListaSolidos* solidos) {
    static int tempo = 0;
    tempo++;

    for (int i = 0; i < NUM_ANIMAIS; i++) {
        Animal* a = &animais[i];

        // Anda por um tempo, para um pouco, e repete
        a->andando = (((tempo + a->fase) / 90) % 3) != 0;

        if (a->andando) {
            float prox_x = a->x + a->vel * a->dir;
            float hx = prox_x + a->hit_x, hy = a->y + a->hit_y;

            if (colisao(hx, hy, a->hit_w, a->hit_h,
                p->x + p->hit_x, p->y + p->hit_y + p->hit_h - 10, p->hit_w, 10)) {
                // Jogador na frente: para e espera (se entrasse nele, o jogador ficaria preso)
                a->andando = 0;
            }
            else if (ha_solido_em(solidos, hx, hy, a->hit_w, a->hit_h, &a->x)) {
                // Objeto na frente: dá meia-volta
                a->dir = -a->dir;
            }
            else {
                a->x = prox_x;
                if (a->x <= a->lim_esq) { a->x = (float)a->lim_esq; a->dir = 1; }
                if (a->x >= a->lim_dir) { a->x = (float)a->lim_dir; a->dir = -1; }
            }
        }

        a->cont++;
        if (a->cont >= (a->andando ? 15 : 40)) {
            a->frame = 1 - a->frame;
            a->cont = 0;
        }
    }
}

void desenhar_animal(Animal* a) {
    int flip = (a->dir > 0) ? ALLEGRO_FLIP_HORIZONTAL : 0;   // os sprites olham para a esquerda

    al_draw_scaled_bitmap(
        a->spr, a->frame * 32, a->andando ? 32 : 0, 32, 32,
        a->x, a->y, 64, 64, flip
    );
}

// Piso e parede de fundo (tiles de 16px desenhados em 32px)
void desenhar_interior_fundo(ALLEGRO_BITMAP* piso, ALLEGRO_BITMAP* parede_topo, ALLEGRO_BITMAP* parede_base) {
    int tile = (int)(16 * ESCALA_MOVEIS);   // 32
    int sw = al_get_bitmap_width(piso);
    int sh = al_get_bitmap_height(piso);

    // Piso
    for (int yy = SALA_CHAO_Y; yy < SALA_BASE_Y; yy += tile) {
        for (int xx = SALA_X; xx < SALA_X + SALA_LARGURA; xx += tile) {
            al_draw_scaled_bitmap(piso, 0, 0, sw, sh, xx, yy, tile, tile, 0);
        }
    }

    // Parede de fundo (2 linhas)
    for (int xx = SALA_X; xx < SALA_X + SALA_LARGURA; xx += tile) {
        al_draw_scaled_bitmap(parede_topo, 0, 0, sw, sh, xx, SALA_PAREDE_Y, tile, tile, 0);
        al_draw_scaled_bitmap(parede_base, 0, 0, sw, sh, xx, SALA_PAREDE_Y + tile, tile, tile, 0);
    }

    // Contorno da sala
    al_draw_rectangle(SALA_X, SALA_PAREDE_Y, SALA_X + SALA_LARGURA, SALA_BASE_Y, al_map_rgb(30, 20, 10), 4.0);
}

// Desenha os móveis antes ou depois do jogador conforme a posição dos pés (ordenação por Y)
void desenhar_moveis(Movel moveis[], int n, float pes_y, int depois_do_player) {
    for (int i = 0; i < n; i++) {
        Movel* m = &moveis[i];

        int atras = 0;
        if (m->plano) atras = 1;
        else if ((m->y + m->h) <= pes_y) atras = 1;

        if ((!depois_do_player && atras) || (depois_do_player && !atras)) {
            al_draw_scaled_bitmap(
                m->bmp, 0, 0, al_get_bitmap_width(m->bmp), al_get_bitmap_height(m->bmp),
                m->x, m->y, m->w, m->h, 0
            );
        }
    }
}

// Dígitos 3x5 desenhados com retângulos (o jogo não usa fonte)
static const unsigned char DIGITOS_3X5[10][5] = {
    { 7,5,5,5,7 },{ 2,6,2,2,7 },{ 7,1,7,4,7 },{ 7,1,7,1,7 },{ 5,5,7,1,1 },
    { 7,4,7,1,7 },{ 7,4,7,5,7 },{ 7,1,1,1,1 },{ 7,5,7,5,7 },{ 7,5,7,1,7 }
};

static void desenhar_numero(float x, float y, int n, int esc) {
    char txt[16];
    snprintf(txt, sizeof(txt), "%d", n);

    for (int passo = 0; passo < 2; passo++) {   // passo 0 = sombra, passo 1 = número
        ALLEGRO_COLOR cor = passo == 0 ? al_map_rgb(0, 0, 0) : al_map_rgb(255, 255, 255);
        float desl = passo == 0 ? (float)esc : 0.0f;

        for (int i = 0; txt[i]; i++) {
            int d = txt[i] - '0';
            for (int r = 0; r < 5; r++) {
                for (int col = 0; col < 3; col++) {
                    if (DIGITOS_3X5[d][r] & (4 >> col)) {
                        float px = x + desl + (i * 4 + col) * esc;
                        float py = y + desl + r * esc;
                        al_draw_filled_rectangle(px, py, px + esc, py + esc, cor);
                    }
                }
            }
        }
    }
}
void desenhar_hud(int slot_selecionado, ALLEGRO_BITMAP* icone_ferro, int qtd_ferro) {
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

    // Slot 1: minério de ferro carregado (aparece em qualquer cena)
    if (qtd_ferro > 0 && icone_ferro) {
        al_draw_scaled_bitmap(icone_ferro, 0, 0, 12, 12, inicio_x + 7, inicio_y + 7, 36, 36, 0);

        int digitos = 1;
        for (int t = qtd_ferro; t >= 10; t /= 10) digitos++;
        float larg = (digitos * 4 - 1) * 2.0f;
        desenhar_numero(inicio_x + TAMANHO_SLOT - 4 - larg, inicio_y + TAMANHO_SLOT - 4 - 10.0f, qtd_ferro, 2);
    }
}