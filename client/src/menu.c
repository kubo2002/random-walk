#include "../headers/menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void menu_print_main() {
    printf("\n=== NAHODNA POCHOZKA ===\n");
    printf("1. Nova simulacia\n");
    printf("2. Pripojit sa k simulacii\n");
    printf("3. Opatovne spustenie simulacie\n");
    printf("4. Koniec\n");
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
    
    printf("Typ sveta (0 = bez prekazok, 1 = s prekazkami, 2 = nacitat zo suboru): ");
    scanf("%d", world_type);
    
    if (*world_type == 2) {
        printf("Meno suboru so svetom: ");
        scanf("%s", world_file);
        params->world_w = 0; // server si zisti zo suboru
        params->world_h = 0;
    } else {
        printf("Sirka sveta: ");
        scanf("%u", &params->world_w);
        printf("Vyska sveta: ");
        scanf("%u", &params->world_h);
        world_file[0] = '\0';
    }
    
    printf("Pocet replikacii: ");
    scanf("%u", &params->replications);
    
    printf("Maximalny pocet krokov (K): ");
    scanf("%u", &params->K);
    
    printf("Pravdepodobnosti pohybu (sucet musi byt 1000 promile):\n");
    printf("  Hore: "); scanf("%u", &params->p_up);
    printf("  Dole: "); scanf("%u", &params->p_down);
    printf("  Vlavo: "); scanf("%u", &params->p_left);
    printf("  Vpravo: "); scanf("%u", &params->p_right);
    
    printf("Meno suboru pre ulozenie vysledkov: ");
    scanf("%s", save_file);
    
    printf("Pociatocny mod (0 = interaktivny, 1 = sumarny): ");
    scanf("%u", &params->mode);
    params->view = 0; // default avg_steps
}

void menu_get_restart_params(char* load_file, uint32_t* replications, char* save_file) {
    printf("\n--- Opatovne spustenie simulacie ---\n");
    printf("Meno suboru s ulozenou simulaciou: ");
    scanf("%s", load_file);
    printf("Pocet replikacii: ");
    scanf("%u", replications);
    printf("Meno suboru pre nove vysledky: ");
    scanf("%s", save_file);
}