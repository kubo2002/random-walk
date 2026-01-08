#include "simulation.h"
#include <stdlib.h> // rand()

/* vyber smeru podla pravdepodobnosti v promile (suma 1000) */
static int choose_direction(const move_probs_t* p)
{
    int r = rand() % 1000;

    if (r < (int)p->p_up) return 0; // up
    r -= (int)p->p_up;

    if (r < (int)p->p_down) return 1; // down
    r -= (int)p->p_down;

    if (r < (int)p->p_left) return 2; // left

    return 3; // right
}

static int wrap(int v, int max)
{
    // wrap-around do rozsahu 0..max-1
    if (v < 0) {
        return max - 1;
    }
    if (v >= max) {
        return 0;
    }
    return v;
}

int simulate_walking(
    int start_x,
    int start_y,
    int w,
    int h,
    int center_x,
    int center_y,
    const move_probs_t* probs,
    int max_steps
)
{
    int x = start_x;
    int y = start_y;

    for (int steps = 0; steps <= max_steps; steps++) {

        if (x == center_x && y == center_y) {
            return steps; // uz som v strede
        }

        int dir = choose_direction(probs);

        if (dir == 0) y++;       // up
        else if (dir == 1) y--;  // down
        else if (dir == 2) x--;  // left
        else x++;                // right

        // wrap-around
        x = wrap(x, w);
        y = wrap(y, h);
    }

    // fail-safe: nedostal sa do stredu
    return max_steps + 1;
}
