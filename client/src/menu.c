#include "../headers/menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void menu_print_main() {
    printf("\n--- MENU ---\n");
    printf("1. Nova simulacia\n");
    printf("2. Opatovne spustenie simulacie\n");
    printf("3. Koniec\n");
    printf("Vasa volba: ");
}

int menu_get_choice() {
    int choice;
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n'); // vycisti buffer
        return -1;
    }
    return choice;
}

void menu_get_sim_params(start_sim_t* params, char* world_file, char* save_file, int* world_type) {
    printf("\n--- Nastavenia novej simulacie ---\n");

    printf("Mod (0 = interaktivny, 1 = sumarny): ");
    scanf("%u", &params->mode);

    printf("Svet (0 = bez prekazok, 1 = s prekazkami, 2 = zo suboru): ");
    scanf("%d", world_type);
    
    if (*world_type == 2) {
        printf("Meno suboru: ");
        scanf("%s", world_file);
        params->world_w = 0;
        params->world_h = 0;
    } else {
        printf("Sirka: ");
        scanf("%u", &params->world_w);
        printf("Vyska: ");
        scanf("%u", &params->world_h);
        world_file[0] = '\0';
    }
    
    printf("Pocet replikacii: ");
    scanf("%u", &params->replications);
    
    printf("Max krokov (K): ");
    scanf("%u", &params->K);
    
    uint32_t total_p = 0;
    while (total_p != 1000) {
        printf("Sance na pohyb (sucet musi byt 1000 promile):\n");
        printf("  Hore: "); scanf("%u", &params->p_up);
        printf("  Dole: "); scanf("%u", &params->p_down);
        printf("  Vlavo: "); scanf("%u", &params->p_left);
        printf("  Vpravo: "); scanf("%u", &params->p_right);
        
        total_p = params->p_up + params->p_down + params->p_left + params->p_right;
        if (total_p != 1000) {
            printf("Zle! Sucet je %u, musi byt 1000. Skus znova.\n", total_p);
        }
    }
    
    printf("Kam ulozime vysledky: ");
    scanf("%s", save_file);
    
    params->start_x = 0; 
    params->start_y = 0;
    params->view = 0;
}

void menu_get_restart_params(char* load_file, uint32_t* replications, char* save_file) {
    printf("\n--- Restart simulacie ---\n");
    printf("Subor so stavom: ");
    scanf("%s", load_file);
    printf("Novy pocet replikacii: ");
    scanf("%u", replications);
    printf("Novy subor pre vysledky: ");
    scanf("%s", save_file);
}