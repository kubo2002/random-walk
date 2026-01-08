#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdint.h>
#include "world.h"

typedef struct {
    uint32_t p_up;
    uint32_t p_down;
    uint32_t p_left;
    uint32_t p_right;
} move_probs_t;

/*
    Jedna nahodna pochodzka v svete w x h.
    Repektuje prekazky a wrap-around (len ak nie su prekazky).

    Vrati:
    - pocet krokov, ak sa dostal do stredu
    - max_steps+1, ak sa do stredu nedostal v limite
*/
int simulate_walking(
    int start_x,
    int start_y,
    const world_t* world,
    bool has_obstacles,
    const move_probs_t* probs,
    int max_steps
);

void simulate_one_step(int* x, int* y, const world_t* world, bool has_obstacles, const move_probs_t* probs);

#endif
