//
// Created by kubo on 1/6/26.
//

#ifndef RANDOM_WALK_MENU_H
#define RANDOM_WALK_MENU_H

#include "../../common/protocol.h"

// Typy poloziek v menu
typedef enum {
    MENU_NEW_SIM = 1,
    MENU_JOIN_SIM = 2,
    MENU_RESTART_SIM = 3,
    MENU_EXIT = 4
} menu_choice_t;

// Funkcie menu
void menu_print_main();
int menu_get_choice();

// Funkcia na nacitanie parametrov novej simulacie
void menu_get_sim_params(start_sim_t* params, char* world_file, char* save_file, int* world_type);

// Funkcia na nacitanie parametrov opätovného spustenia
void menu_get_restart_params(char* load_file, uint32_t* replications, char* save_file);

#endif //RANDOM_WALK_MENU_H