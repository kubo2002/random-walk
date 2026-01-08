#ifndef RANDOM_WALK_WORLD_H
#define RANDOM_WALK_WORLD_H

#include <stdint.h>
#include <stdbool.h>

// Typy policok v nasom svete
typedef enum {
    CELL_EMPTY = 0,
    CELL_OBSTACLE = 1
} cell_type_t;

// Struktura sveta
typedef struct {
    int width;
    int height;
    uint8_t* grid; // Dynamicke pole (width * height)
} world_t;

// Funkcie pre pracu so svetom
world_t* world_create(int w, int h);
void world_free(world_t* world);

// Nastavenie policka
void world_set_cell(world_t* world, int x, int y, cell_type_t type);
cell_type_t world_get_cell(world_t* world, int x, int y);

// Nahodne generovanie prekazok
void world_generate_obstacles(world_t* world, double density);

// Overenie ci je kazde volne policko schopne dosiahnut stred [0,0]
// Podla zadania sa prekazka nemoze vyskytovat na [0,0]
bool world_check_reachability(world_t* world);

// Praca so subormi
int world_save_to_file(world_t* world, const char* filename);
world_t* world_load_from_file(const char* filename);

#endif //RANDOM_WALK_WORLD_H
