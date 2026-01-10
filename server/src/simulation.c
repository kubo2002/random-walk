#include "simulation.h"
#include <stdlib.h> // rand()

// Vyberieme smer podla pravdepodobnosti (v promile)
static int choose_direction(const move_probs_t* p)
{
    int r = rand() % 1000;

    if (r < (int)p->p_up) return 0;    // hore
    r -= (int)p->p_up;

    if (r < (int)p->p_down) return 1;  // dole
    r -= (int)p->p_down;

    if (r < (int)p->p_left) return 2;  // vlavo

    return 3; // vpravo
}

// Aby chodec nevysiel z mapy, ked nie su prekazky
static int wrap(int v, int max)
{
    if (v < 0) return max - 1;
    if (v >= max) return 0;
    return v;
}

// Jeden krok chodca
void simulate_one_step(int* x, int* y, const world_t* world, bool has_obstacles, const move_probs_t* probs)
{
    int dir = choose_direction(probs);
    int nx = *x;
    int ny = *y;

    if (dir == 0) ny--;       // hore
    else if (dir == 1) ny++;  // dole
    else if (dir == 2) nx--;  // vlavo
    else nx++;                // vpravo

    if (has_obstacles) {
        // Ak su prekazky, tak cez ne neprejdeme
        if (nx >= 0 && nx < world->width && ny >= 0 && ny < world->height) {
            if (world_get_cell((world_t*)world, nx, ny) == CELL_EMPTY) {
                *x = nx;
                *y = ny;
            }
        }
    } else {
        // Bez prekazok sa objavime na druhej strane
        *x = wrap(nx, world->width);
        *y = wrap(ny, world->height);
    }
}

// Cela pochodzka az do stredu
int simulate_walking(int start_x, int start_y, const world_t* world, bool has_obstacles, const move_probs_t* probs, int max_steps)
{
    int x = start_x;
    int y = start_y;
    int target_x = world->width / 2;
    int target_y = world->height / 2;

    for (int steps = 0; steps <= max_steps; steps++) {
        if (x == target_x && y == target_y) {
            return steps;
        }
        simulate_one_step(&x, &y, world, has_obstacles, probs);
    }

    return max_steps + 1;
}