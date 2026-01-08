#include "simulation.h"
#include <stdlib.h> // rand()

/* vyber smeru podla pravdepodobnosti v promile (suma 1000) */
static int choose_direction(const move_probs_t* p)
{
    int r = rand() % 1000;

    if (r < (int)p->p_up) return 0; // hore
    r -= (int)p->p_up;

    if (r < (int)p->p_down) return 1; // dole
    r -= (int)p->p_down;

    if (r < (int)p->p_left) return 2; // vlavo

    return 3; // vpravo
}

static int wrap(int v, int max)
{
    if (v < 0) return max - 1;
    if (v >= max) return 0;
    return v;
}

void simulate_one_step(int* x, int* y, const world_t* world, bool has_obstacles, const move_probs_t* probs)
{
    int dir = choose_direction(probs);
    int nx = *x;
    int ny = *y;

    if (dir == 0) ny++;       // up
    else if (dir == 1) ny--;  // down
    else if (dir == 2) nx--;  // left
    else nx++;                // right

    if (has_obstacles) {
        // Svet s prekazkami - nema wrap-around, ak narazi na prekazku alebo kraj, stoji
        if (nx >= 0 && nx < world->width && ny >= 0 && ny < world->height) {
            if (world_get_cell((world_t*)world, nx, ny) == CELL_EMPTY) {
                *x = nx;
                *y = ny;
            }
        }
    } else {
        // Svet bez prekazok - wrap-around podla zadania
        // Ak sa pokusa ist mimo mapu, objavi sa na opacnej strane
        *x = wrap(nx, world->width);
        *y = wrap(ny, world->height);
    }
}

int simulate_walking(
    int start_x,
    int start_y,
    const world_t* world,
    bool has_obstacles,
    const move_probs_t* probs,
    int max_steps
)
{
    int x = start_x;
    int y = start_y;

    for (int steps = 0; steps <= max_steps; steps++) {
        // [0,0] je stred sveta podla zadania
        if (x == 0 && y == 0) {
            return steps;
        }

        simulate_one_step(&x, &y, world, has_obstacles, probs);
    }

    return max_steps + 1;
}