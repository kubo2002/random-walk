#include "../headers/world.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Vytvori novy svet, alokuje pamat
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

// dealokuje
void world_free(world_t* world) {
    if (world) {
        if (world->grid) free(world->grid);
        free(world);
    }
}

// Nastavi co je na danych suradniciach
void world_set_cell(world_t* world, int x, int y, cell_type_t type) {
    if (x >= 0 && x < world->width && y >= 0 && y < world->height) {
        world->grid[y * world->width + x] = (uint8_t)type;
    }
}

// Zisti co je na danych suradniciach
cell_type_t world_get_cell(world_t* world, int x, int y) {
    if (x >= 0 && x < world->width && y >= 0 && y < world->height) {
        return (cell_type_t)world->grid[y * world->width + x];
    }
    return CELL_OBSTACLE; // Mimo mapy je prekazka
}

// Nahadze tam nejake prekazky
void world_generate_obstacles(world_t* world, double density) {
    memset(world->grid, 0, world->width * world->height);
    
    int target_x = world->width / 2;
    int target_y = world->height / 2;

    for (int y = 0; y < world->height; y++) {
        for (int x = 0; x < world->width; x++) {
            // Stred musi byt volny!
            if (x == target_x && y == target_y) continue;
            
            double r = (double)rand() / RAND_MAX;
            if (r < density) {
                world_set_cell(world, x, y, CELL_OBSTACLE);
            }
        }
    }
}

// BFS - skontroluje, ci sa zo vsetkych prazdnych miest da dostat do ciela
bool world_check_reachability(world_t* world) {
    int size = world->width * world->height;
    bool* visited = calloc(size, sizeof(bool));
    if (!visited) return false;

    int* queue_x = malloc(size * sizeof(int));
    if (!queue_x) {
        free(visited);
        return false;
    }
    int* queue_y = malloc(size * sizeof(int));
    if (!queue_y) {
        free(visited);
        free(queue_x);
        return false;
    }
    
    int head = 0, tail = 0;
    int target_x = world->width / 2;
    int target_y = world->height / 2;

    // Ideme od ciela a pozerame kam sa da dostat
    queue_x[tail] = target_x;
    queue_y[tail] = target_y;
    tail++;
    visited[target_y * world->width + target_x] = true;
    
    int dx[] = {0, 0, -1, 1};
    int dy[] = {1, -1, 0, 0};
    
    while (head < tail) {
        int cx = queue_x[head];
        int cy = queue_y[head];
        head++;
        
        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            
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
    
    // Su vsetky volne policka navstivene?
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

// Ulozi mapu do suboru
int world_save_to_file(world_t* world, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return -1;
    
    fwrite(&world->width, sizeof(int), 1, f);
    fwrite(&world->height, sizeof(int), 1, f);
    fwrite(world->grid, sizeof(uint8_t), world->width * world->height, f);
    
    fclose(f);
    return 0;
}

// Nacita mapu zo suboru
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
