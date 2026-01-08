#include "../headers/world.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Vytvori novy svet danej velkosti
world_t* world_create(int w, int h) {
    world_t* world = (world_t*)malloc(sizeof(world_t));
    if (!world) return NULL;
    
    world->width = w;
    world->height = h;
    world->grid = (uint8_t*)calloc(w * h, sizeof(uint8_t));
    
    if (!world->grid) {
        free(world);
        return NULL;
    }
    
    return world;
}

// Uvolni pamat sveta
void world_free(world_t* world) {
    if (world) {
        if (world->grid) free(world->grid);
        free(world);
    }
}

// Nastavi typ policka na danych suradniciach
void world_set_cell(world_t* world, int x, int y, cell_type_t type) {
    if (x >= 0 && x < world->width && y >= 0 && y < world->height) {
        world->grid[y * world->width + x] = (uint8_t)type;
    }
}

// Ziska typ policka na danych suradniciach
cell_type_t world_get_cell(world_t* world, int x, int y) {
    if (x >= 0 && x < world->width && y >= 0 && y < world->height) {
        return (cell_type_t)world->grid[y * world->width + x];
    }
    return CELL_OBSTACLE; // Mimo sveta povazujeme za prekazku (ak nie je wrap-around)
}

// Nahodne vygeneruje prekazky s danou hustotou
void world_generate_obstacles(world_t* world, double density) {
    // Najprv vsetko vymazeme
    memset(world->grid, 0, world->width * world->height);
    
    for (int y = 0; y < world->height; y++) {
        for (int x = 0; x < world->width; x++) {
            // Stred [0,0] nesmie mat prekazku
            if (x == 0 && y == 0) continue;
            
            double r = (double)rand() / RAND_MAX;
            if (r < density) {
                world_set_cell(world, x, y, CELL_OBSTACLE);
            }
        }
    }
}

// BFS na overenie, ci sa da zo vsetkych prazdnych policok dostat do [0,0]
bool world_check_reachability(world_t* world) {
    int size = world->width * world->height;
    bool* visited = (bool*)calloc(size, sizeof(bool));
    int* queue_x = (int*)malloc(size * sizeof(int));
    int* queue_y = (int*)malloc(size * sizeof(int));
    
    int head = 0, tail = 0;
    
    // Zacneme v [0,0] a ideme "opacne" - kam sa vsade da dostat zo stredu
    queue_x[tail] = 0;
    queue_y[tail] = 0;
    tail++;
    visited[0] = true;
    
    int dx[] = {0, 0, -1, 1};
    int dy[] = {1, -1, 0, 0};
    
    while (head < tail) {
        int cx = queue_x[head];
        int cy = queue_y[head];
        head++;
        
        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            
            // Wrap-around podla zadania (svet bez prekazok ma wrap-around, 
            // u sveta s prekazkami nie je explicitne povedane, ale byva to bez wrap-around
            // alebo s nim. Ponechajme bez wrap-around pre prekazky pre jednoduchost 
            // alebo implementujme podla potreby. Zadanie hovori: "svet bez prekazok - objavi sa na opacnom konci"
            // Pri svete s prekazkami to nepise. Skusme bez wrapu pre prekazky.)
            
            if (nx >= 0 && nx < world->width && ny >= 0 && ny < world->height) {
                int idx = ny * world->width + nx;
                if (!visited[idx] && world->grid[idx] == CELL_EMPTY) {
                    visited[idx] = true;
                    queue_x[tail] = nx;
                    queue_y[tail] = ny;
                    tail++;
                }
            }
        }
    }
    
    // Skontrolujeme ci sme navstivili vsetky EMPTY policka
    bool all_reachable = true;
    for (int i = 0; i < size; i++) {
        if (world->grid[i] == CELL_EMPTY && !visited[i]) {
            all_reachable = false;
            break;
        }
    }
    
    free(visited);
    free(queue_x);
    free(queue_y);
    
    return all_reachable;
}

// Ulozenie sveta do suboru
int world_save_to_file(world_t* world, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return -1;
    
    fwrite(&world->width, sizeof(int), 1, f);
    fwrite(&world->height, sizeof(int), 1, f);
    fwrite(world->grid, sizeof(uint8_t), world->width * world->height, f);
    
    fclose(f);
    return 0;
}

// Nacitanie sveta zo suboru
world_t* world_load_from_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    int w, h;
    if (fread(&w, sizeof(int), 1, f) != 1) { fclose(f); return NULL; }
    if (fread(&h, sizeof(int), 1, f) != 1) { fclose(f); return NULL; }
    
    world_t* world = world_create(w, h);
    if (!world) { fclose(f); return NULL; }
    
    if (fread(world->grid, sizeof(uint8_t), w * h, f) != (size_t)(w * h)) {
        world_free(world);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    return world;
}
