#include <stdio.h>
#include <stdlib.h>
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
#define ESCALA_ARVORE_PEQ   3.0f
#define ESCALA_CASA         3.0f
#define ESCALA_MOVEIS       2.0f   // escala dos móveis/tiles do interior

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
#define MAX_SOLIDOS      64    // máximo de objetos sólidos por cena
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
typedef enum { ITEM_CENARIO, ITEM_ANIMAL, ITEM_SLIME, ITEM_PLAYER } TipoItem;

typedef struct {
    float base_y;
    TipoItem tipo;
    int indice;
} ItemDesenho;

typedef enum { CENA_EXTERIOR, CENA_INTERIOR } Cena;


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
void init_slimes(Slime slimes[]);

void atualizar_player(Player* p, Slime slimes[], int n_slimes,
    const ListaSolidos* solidos, Retangulo area);
void atualizar_slimes(Slime slimes[], Player* p, const ListaSolidos* solidos);

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
void desenhar_exterior(const ListaCenario* cenario, Animal animais[], Slime slimes[],
    ALLEGRO_BITMAP* slime_sprite, Player* p, ALLEGRO_BITMAP* personagem);
void desenhar_interior_fundo(ALLEGRO_BITMAP* piso, ALLEGRO_BITMAP* parede_topo, ALLEGRO_BITMAP* parede_base);
void desenhar_moveis(Movel moveis[], int n, float pes_y, int depois_do_player);
void desenhar_hud(int slot_selecionado);


// ============================================================
// FUNÇÃO PRINCIPAL (MAIN)
// ============================================================

int main() {

    ALLEGRO_DISPLAY* janela = NULL;
    ALLEGRO_EVENT_QUEUE* fila = NULL;
    ALLEGRO_TIMER* timer = NULL;

    // Bitmaps do exterior
    ALLEGRO_BITMAP* personagem = NULL;
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
    Slime slimes[NUM_SLIMES];
    Animal animais[NUM_ANIMAIS];

    // Cena atual e dados da casa
    Cena cena = CENA_EXTERIOR;
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
    timer = al_create_timer(1.0 / 60.0);
    fila = al_create_event_queue();

    // Carregamento de Sprites do exterior
    personagem = al_load_bitmap("Assets/Player.png");
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

    if (!personagem || !grama || !caminho || !agua || !slime_sprite ||
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

            if (fade_estado == 0) {
                // Jogo normal
                if (cena == CENA_EXTERIOR) {
                    atualizar_player(&player, slimes, NUM_SLIMES, &solidos_ext, tela_ext);
                    atualizar_slimes(slimes, &player, &solidos_ext);
                    atualizar_animais(animais, &player, &solidos_ext);

                    // Entrar na casa: basta pisar na porta
                    if (colisao_pes_rect(&player, porta_ext, player.x, player.y)) {
                        cena_destino = CENA_INTERIOR;
                        fade_estado = 1;
                    }
                }
                else {
                    atualizar_player(&player, slimes, 0, &solidos_int, sala);

                    // Sair da casa: basta pisar no capacho
                    if (colisao_pes_rect(&player, porta_int, player.x, player.y)) {
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
                    cena = cena_destino;

                    if (cena == CENA_INTERIOR) {
                        player.x = 640 - (player.hit_x + player.hit_w / 2);
                        player.y = 470;
                        player.ultima_direcao = 2;
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

                // 2. Cenário, animais, slimes e jogador, em ordem de profundidade
                desenhar_exterior(&cenario, animais, slimes, slime_sprite, &player, personagem);

                // 3. Barras de vida dos slimes (por cima de tudo, para não sumirem atrás das árvores)
                for (int i = 0; i < NUM_SLIMES; i++) {
                    if (slimes[i].vida <= 0) continue;

                    desenhar_barra_vida(
                        slimes[i].x + (64 - LARGURA_BARRA_SLIME) / 2.0f,
                        slimes[i].y + 10,
                        LARGURA_BARRA_SLIME, ALTURA_BARRA_SLIME,
                        slimes[i].vida, VIDA_MAXIMA_SLIME
                    );
                }

                // DEBUG: descomente para ver as hitboxes
                // (vermelho = objetos sólidos, verde = gatilho da porta)
                /*
                for (int k = 0; k < solidos_ext.n; k++) {
                    Retangulo c = caixa_solido(&solidos_ext.itens[k]);
                    al_draw_rectangle(c.x, c.y, c.x + c.w, c.y + c.h, al_map_rgb(255, 0, 0), 2);
                }
                al_draw_rectangle(porta_ext.x, porta_ext.y, porta_ext.x + porta_ext.w, porta_ext.y + porta_ext.h, al_map_rgb(0, 255, 0), 2);
                */
            }
            else {
                // Interior: piso e paredes, depois os móveis que ficam ATRÁS do jogador
                desenhar_interior_fundo(int_piso, int_parede_topo, int_parede_base);
                desenhar_moveis(moveis, n_moveis, pes_y, 0);

                // Jogador e os móveis que ficam NA FRENTE dele
                desenhar_player(&player, personagem);
                desenhar_moveis(moveis, n_moveis, pes_y, 1);
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

    al_destroy_bitmap(personagem);
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

void atualizar_slimes(Slime slimes[], Player* p, const ListaSolidos* solidos) {
    for (int i = 0; i < NUM_SLIMES; i++) {
        if (slimes[i].vida <= 0) continue;

        float prox_x = slimes[i].x + slimes[i].velocidade * slimes[i].direcao;
        Slime teste = slimes[i];
        teste.x = prox_x;

        // Bateu no jogador?
        int bateu = colisao_player_slime(p, &teste, p->x, p->y);

        // Bateu em algum objeto sólido (casa, árvores, etc.)?
        if (!bateu) {
            bateu = ha_solido_em(solidos, teste.x + teste.hit_x, teste.y + teste.hit_y, teste.hit_w, teste.hit_h, NULL);
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
void desenhar_exterior(const ListaCenario* cenario, Animal animais[], Slime slimes[],
    ALLEGRO_BITMAP* slime_sprite, Player* p, ALLEGRO_BITMAP* personagem) {

    ItemDesenho fila[MAX_SPRITES + NUM_ANIMAIS + NUM_SLIMES + 1];
    int n = 0;

    for (int i = 0; i < cenario->n; i++) {
        fila[n++] = (ItemDesenho){ cenario->itens[i].base_y, ITEM_CENARIO, i };
    }
    for (int i = 0; i < NUM_ANIMAIS; i++) {
        fila[n++] = (ItemDesenho){ animais[i].y + animais[i].hit_y + animais[i].hit_h, ITEM_ANIMAL, i };
    }
    for (int i = 0; i < NUM_SLIMES; i++) {
        if (slimes[i].vida <= 0) continue;
        fila[n++] = (ItemDesenho){ slimes[i].y + slimes[i].hit_y + slimes[i].hit_h, ITEM_SLIME, i };
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
        case ITEM_SLIME:  desenhar_slime(&slimes[fila[i].indice], slime_sprite); break;
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

    int flip = (p->ultima_direcao == 3) ? ALLEGRO_FLIP_HORIZONTAL : 0;
    int coluna = p->coluna, linha = p->linha;

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
        int atras = m->plano || (m->y + m->h) <= pes_y;

        if ((!depois_do_player && atras) || (depois_do_player && !atras)) {
            al_draw_scaled_bitmap(
                m->bmp, 0, 0, al_get_bitmap_width(m->bmp), al_get_bitmap_height(m->bmp),
                m->x, m->y, m->w, m->h, 0
            );
        }
    }
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