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

// Pomocne premenne pre tabulky
static int g_w = 0;
static int g_h = 0;
static int32_t* g_cells_avg = NULL;
static int32_t* g_cells_prob = NULL;
static int g_got_any_cell = 0;

// Premenne pre interakciu
static int g_iw = 0;
static int g_ih = 0;
static char* g_grid = NULL;
static int g_has_pos = 0;
static int g_last_x = 0;
static int g_last_y = 0;

// Uvolni pamat tabuliek
static void free_cells(void)
{
    if (g_cells_avg) free(g_cells_avg);
    if (g_cells_prob) free(g_cells_prob);
    g_cells_avg = NULL;
    g_cells_prob = NULL;
    g_w = 0; g_h = 0;
}

// Priprava na summary mod
void render_summary_begin(int w, int h, int view)
{
    (void)view;
    free_cells();
    if (w <= 0 || h <= 0) {
        return;
    }

    g_w = w; g_h = h;
    g_cells_avg = (int32_t*)malloc((size_t)g_w * (size_t)g_h * sizeof(int32_t));
    g_cells_prob = (int32_t*)malloc((size_t)g_w * (size_t)g_h * sizeof(int32_t));
    
    for (int i = 0; i < g_w * g_h; i++) {
        g_cells_avg[i] = -1;
        g_cells_prob[i] = -1;
    }
    g_got_any_cell = 0;
}

// Prisla bunka zo servera
void render_summary_cell(int x, int y, int value_fixed)
{
    if (!g_cells_avg) return;
    if (x < 0 || x >= g_w || y < 0 || y >= g_h) return;

    int idx = y * g_w + x;
    if (value_fixed == -2) {
        g_cells_avg[idx] = -2;
        g_cells_prob[idx] = -2;
    } else if (value_fixed >= 2000000) {
        g_cells_prob[idx] = value_fixed - 2000000;
    } else {
        g_cells_avg[idx] = value_fixed;
    }
    g_got_any_cell = 1;
}

// Vykresli tabulky
void render_summary_end(int view)
{
    (void)view;
    if (!g_got_any_cell || !g_cells_avg) return;

    for (int v_mode = 0; v_mode <= 1; v_mode++) {
        if (v_mode == 0) printf("\n--- PRIEMERNE KROKY ---\n");
        else printf("\n--- PRAVDEPODOBNOST DO K KROKOV ---\n");

        printf("     ");
        for (int x = 0; x < g_w; x++) printf("%8d", x);
        printf("\n     ");
        for (int x = 0; x < g_w; x++) printf("--------");
        printf("\n");

        for (int y = 0; y < g_h; y++) {
            printf("%3d |", y);
            for (int x = 0; x < g_w; x++) {
                int idx = y * g_w + x;
                int32_t v = (v_mode == 0) ? g_cells_avg[idx] : g_cells_prob[idx];
                if (v == -2) printf("%8s", "O");
                else if (v < 0) printf("%8s", ".");
                else {
                    if (v_mode == 0) printf("%8.2f", (double)v / 100.0);
                    else printf("%8.4f", (double)v / 10000.0);
                }
            }
            printf("\n");
        }
    }
    printf("\n");
}

void render_summary_final(void) {
    free_cells();
}

// Interakcia - uvolnenie
static void interactive_free(void)
{
    if (g_grid) free(g_grid);
    g_grid = NULL;
    g_iw = g_ih = 0;
    g_has_pos = 0;
}

// Nakresli mapu do terminalu
static void interactive_draw(void)
{
    if (!g_grid) return;
    printf("\033[H\033[J"); // Vymaz obrazovku

    for (int yy = 0; yy < g_ih; yy++) {
        for (int xx = 0; xx < g_iw; xx++) {
            putchar(g_grid[yy * g_iw + xx]);
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

// Start interakcie
void render_interactive_begin(int w, int h)
{
    interactive_free();
    if (w <= 0 || h <= 0) return;

    g_iw = w; g_ih = h;
    g_grid = (char*)malloc((size_t)g_iw * (size_t)g_ih);
    for (int i = 0; i < g_iw * g_ih; i++) g_grid[i] = '.';

    // Ciel je stred
    if (w/2 < g_iw && h/2 < g_ih) g_grid[(h/2) * g_iw + (w/2)] = '*';
}

// Prisli prekazky
void render_interactive_world_data(const uint8_t* data, uint32_t len)
{
    if (!g_grid || len != (uint32_t)(g_iw * g_ih)) return;
    for (int i = 0; i < g_iw * g_ih; i++) {
        if (data[i] == 1) g_grid[i] = 'O';
    }
    g_grid[(g_ih/2) * g_iw + (g_iw/2)] = '*';
    interactive_draw();
}

// Chodec sa pohol
void render_interactive_step(int x, int y)
{
    if (!g_grid) return;
    int xx = x + (g_iw / 2);
    int yy = y + (g_ih / 2);
    if (xx < 0 || xx >= g_iw || yy < 0 || yy >= g_ih) return;

    if (!g_has_pos) {
        g_grid[yy * g_iw + xx] = 'S'; // Start
    } else {
        int px = g_last_x + (g_iw / 2);
        int py = g_last_y + (g_ih / 2);
        if (px >= 0 && px < g_iw && py >= 0 && py < g_ih) {
            if (px == g_iw/2 && py == g_ih/2) g_grid[py * g_iw + px] = '*';
            else if (g_grid[py * g_iw + px] != 'S') g_grid[py * g_iw + px] = 'x'; 
        }
        if (xx == g_iw/2 && yy == g_ih/2) g_grid[yy * g_iw + xx] = '*';
        else g_grid[yy * g_iw + xx] = 'C'; 
    }
    g_last_x = x; g_last_y = y; g_has_pos = 1;
    interactive_draw();
}

void render_interactive_end(void)
{
    interactive_free();
}

void render_interactive_reset(void)
{
    if (!g_grid) return;
    for (int i = 0; i < g_iw * g_ih; i++) g_grid[i] = '.';
    g_grid[(g_ih/2) * g_iw + (g_iw/2)] = '*';
    g_has_pos = 0;
    interactive_draw();
}