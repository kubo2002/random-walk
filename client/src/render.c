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

static int g_w = 0;
static int g_h = 0;
static int g_cx = 0;
static int g_cy = 0;
static int g_got_any_cell = 0;
static int32_t* g_cells = NULL;

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
    g_cx = w / 2;
    g_cy = h / 2;

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

    int xx = x + g_cx;
    int yy = y + g_cy;

    if (xx < 0 || xx >= g_w || yy < 0 || yy >= g_h) {
        // mimo sveta (nemalo by sa stat)
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
