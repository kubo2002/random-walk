#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    Interny buffer:
    - world_w, world_h su rozmery sveta
    - center_x, center_y su stred sveta (w/2, h/2)
    - cells je pole w*h hodnot (value_fixed)
*/

// premenne pre summary mod
static int g_w = 0;
static int g_h = 0;
static int g_cx = 0;
static int g_cy = 0;
static int g_got_any_cell = 0;
static int32_t* g_cells = NULL;

//premenne pre interaktivny mod
static int g_iw = 0;
static int g_ih = 0;
static int g_icx = 0;
static int g_icy = 0;
static char* g_grid = NULL;   // 2D mapa iw*ih
static int g_has_pos = 0;
static int g_last_x = 0;
static int g_last_y = 0;


static void free_cells(void)
{
    free(g_cells);
    g_cells = NULL;
    g_w = 0;
    g_h = 0;
    g_cx = 0;
    g_cy = 0;
}

void render_summary_begin(int w, int h, int view)
{
    (void)view;

    // reset stareho bufferu
    free_cells();

    g_w = w;
    g_h = h;
    // V novom svete je stred vzdy [0,0], mapujeme suradnice priamo
    g_cx = 0; 
    g_cy = 0;

    g_cells = (int32_t*)malloc((size_t)g_w * (size_t)g_h * sizeof(int32_t));
    if (!g_cells) {
        printf("[client] render: malloc failed\n");
        free_cells();
        return;
    }

    // nastavime default hodnoty (napr. -1 = "neprislo")
    for (int i = 0; i < g_w * g_h; i++) {
        g_cells[i] = -1;
    }

    printf("[client] summary buffer ready (%dx%d)\n", g_w, g_h);
    g_got_any_cell = 0;
}

/*
    x,y su so stredom v (0,0), napr. -5..+5 pri 11x11
    my to mapneme na indexy 0..w-1 / 0..h-1
*/
void render_summary_cell(int x, int y, int value_fixed)
{
    if (!g_cells) return;

    // Suradnice od servera su uz indexy 0..w-1
    int xx = x;
    int yy = y;

    if (xx < 0 || xx >= g_w || yy < 0 || yy >= g_h) {
        return;
    }

    int idx = yy * g_w + xx;
    g_cells[idx] = (int32_t)value_fixed;
    g_got_any_cell = 1;
}

static void print_header(int view)
{
    if (view == 0) {
        printf("\n===== SUMMARY: AVG_STEPS (value_fixed = avg*100) =====\n");
        printf("format: number = value_fixed/100.00\n\n");
    } else {
        printf("\n===== SUMMARY: PROB_K (value_fixed = prob*10000) =====\n");
        printf("format: number = value_fixed/10000.0000\n\n");
    }
}

void render_summary_end(int view)
{
    if (!g_got_any_cell) {
        // neprisla ziadna bunka -> nebudeme tlacit prazdnu tabulku
        printf("[client] summary_end ignored (no cells received)\n");
        return;
    }

    if (!g_cells) {
        return;
    }

    print_header(view);

    /*
        vypis:
        - y vypiseme od +max po -max (aby to vyzeralo ako graf)
        - x ide zlava doprava
    */

    // x os (hlavicka)
    printf("     ");
    for (int x = -g_cx; x <= g_cx; x++) {
        printf("%8d", x);
    }
    printf("\n");

    // ciara
    printf("     ");
    for (int x = -g_cx; x <= g_cx; x++) {
        printf("--------");
    }
    printf("\n");

    for (int y = g_cy; y >= -g_cy; y--) {
        printf("%3d |", y);

        for (int x = -g_cx; x <= g_cx; x++) {
            int xx = x + g_cx;
            int yy = y + g_cy;
            int idx = yy * g_w + xx;

            int32_t v = g_cells[idx];
            if (v < 0) {
                printf("%8s", "NA");
            } else {
                if (view == 0) {
                    // avg_steps * 100
                    double d = (double)v / 100.0;
                    printf("%8.2f", d);
                } else {
                    // prob_k * 10000
                    double d = (double)v / 10000.0;
                    printf("%8.4f", d);
                }
            }
        }
        printf("\n");
    }

    printf("\n===== END SUMMARY =====\n\n");
    free_cells();
}



static void interactive_free(void)
{
    free(g_grid);
    g_grid = NULL;
    g_iw = g_ih = 0;
    g_icx = g_icy = 0;
    g_has_pos = 0;
}

static void interactive_draw(void)
{
    if (!g_grid) return;

    // Vyčistenie obrazovky pre plynulejšiu simuláciu
    printf("\033[H\033[J");

    // vypis od y=+max po y=-max (ako pri summary)
    for (int yy = 0; yy < g_ih; yy++) {
        for (int xx = 0; xx < g_iw; xx++) {
            putchar(g_grid[yy * g_iw + xx]);
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

void render_interactive_begin(int w, int h)
{
    interactive_free();

    g_iw = w;
    g_ih = h;
    g_icx = 0; // Stred je vzdy mapovany na indexy podla servera
    g_icy = 0;

    g_grid = (char*)malloc((size_t)g_iw * (size_t)g_ih);
    if (!g_grid) {
        printf("[client] interactive: malloc failed\n");
        interactive_free();
        return;
    }

    // vypln bodkami
    for (int i = 0; i < g_iw * g_ih; i++) {
        g_grid[i] = '.';
    }

    // oznac ciel (stred) znakom O
    if (g_icx < g_iw && g_icy < g_ih) {
        g_grid[g_icy * g_iw + g_icx] = 'O';
    }

    printf("[client] interaktivna mapa pripravena (%dx%d)\n", g_iw, g_ih);
}

void render_interactive_world_data(const uint8_t* data, uint32_t len)
{
    if (!g_grid || len != (uint32_t)(g_iw * g_ih)) return;

    for (int i = 0; i < g_iw * g_ih; i++) {
        if (data[i] == 1) { // CELL_OBSTACLE
            g_grid[i] = 'X';
        }
    }
    // Ciel vzdy ostane O
    g_grid[g_icy * g_iw + g_icx] = 'O';
    
    interactive_draw();
}

void render_interactive_step(int x, int y)
{
    if (!g_grid) return;

    // Server posiela indexy 0..w-1
    int xx = x;
    int yy = y;

    if (xx < 0 || xx >= g_iw || yy < 0 || yy >= g_ih) return;

    // Zanechávanie stopy: predošlá pozícia ostane označená malým 'x' (stopa)
    // namiesto toho, aby sme ju mazali (bodka).
    if (g_has_pos) {
        int px = g_last_x;
        int py = g_last_y;

        if (px >= 0 && px < g_iw && py >= 0 && py < g_ih) {
            // Ak sme boli na cieli, vrátime O, inak necháme stopu 'x'
            if (px == 0 && py == 0) g_grid[py * g_iw + px] = 'O';
            else g_grid[py * g_iw + px] = 'x'; 
        }
    }

    // Nastav aktuálnu pozíciu chodca
    g_grid[yy * g_iw + xx] = '*';
    g_last_x = x;
    g_last_y = y;
    g_has_pos = 1;

    // Prekresli mapu
    interactive_draw();
}

void render_interactive_end(void)
{
    printf("[client] interactive finished\n");
    interactive_free();
}

void render_interactive_reset(void)
{
    if (!g_grid) return;

    // clear all to '.'
    for (int i = 0; i < g_iw * g_ih; i++) {
        g_grid[i] = '.';
    }

    // goal
    g_grid[g_icy * g_iw + g_icx] = 'O';

    g_has_pos = 0;
    printf("[client] interactive: next replication\n");
    interactive_draw();
}