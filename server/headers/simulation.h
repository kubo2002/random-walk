#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdint.h>

typedef struct {
    uint32_t p_up;
    uint32_t p_down;
    uint32_t p_left;
    uint32_t p_right;
} move_probs_t;

/*
    Jedna nahodna pochodzka v svete w x h s wrap-around.

    start_x, start_y su indexy 0..w-1, 0..h-1
    center_x, center_y je stred sveta (typicky w/2, h/2)

    Vrati:
    - pocet krokov, ak sa dostal do stredu
    - max_steps+1, ak sa do stredu nedostal v limite (fail-safe)
*/
int simulate_walking(
    int start_x,
    int start_y,
    int w,
    int h,
    int center_x,
    int center_y,
    const move_probs_t* probs,
    int max_steps
);

#endif
