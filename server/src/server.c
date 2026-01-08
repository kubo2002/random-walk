#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>

#include "server_net.h"
#include "protocol.h"
#include "protocol_io.h"
#include "simulation.h"
#include "world.h"

// --- Globalne premenne pre stav servera ---
static int g_running = 1;

static int g_client_fd = -1;
static pthread_mutex_t g_client_lock = PTHREAD_MUTEX_INITIALIZER;

static int g_sim_running = 0;
static start_sim_t g_sim_params; 
static world_t* g_world = NULL;
static pthread_mutex_t g_sim_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_sim_cv = PTHREAD_COND_INITIALIZER;

// Pomocna funkcia na poslanie spravy aktualnemu klientovi
static void send_to_client(uint32_t type, const void* payload, uint32_t payload_len) {
    pthread_mutex_lock(&g_client_lock);
    int fd = g_client_fd;
    pthread_mutex_unlock(&g_client_lock);

    if (fd >= 0) {
        protocol_send(fd, type, payload, payload_len);
    }
}

// Funkcia na ulozenie vysledkov do suboru
static void save_results_to_file(const char* filename, int w, int h, double* avg_steps, double* prob_k) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("[server] Chyba pri otvarani suboru pre zapis: %s\n", filename);
        return;
    }

    fprintf(f, "X;Y;AvgSteps;ProbK\n");
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            fprintf(f, "%d;%d;%.2f;%.4f\n", x, y, avg_steps[idx], prob_k[idx]);
        }
    }
    fclose(f);
    printf("[server] Vysledky ulozene do: %s\n", filename);
}

// Vlakno pre simulaciu
static void* simulation_thread(void* arg) {
    (void)arg;
    printf("[server] Simulacne vlakno nastartovane.\n");

    while (g_running) {
        // Caka na signal pre start simulacie
        pthread_mutex_lock(&g_sim_lock);
        while (g_running && g_sim_running == 0) {
            pthread_cond_wait(&g_sim_cv, &g_sim_lock);
        }
        if (!g_running) {
            pthread_mutex_unlock(&g_sim_lock);
            break;
        }

        // Kopirovanie parametrov
        start_sim_t params = g_sim_params;
        world_t* world = g_world;
        pthread_mutex_unlock(&g_sim_lock);

        int w = world->width;
        int h = world->height;
        int reps = params.replications;
        int K = params.K;
        move_probs_t probs = {params.p_up, params.p_down, params.p_left, params.p_right};
        bool has_obstacles = (params.world_type != 0);

        // Priprava poli pre statistiky (pre sumarny mod)
        double* total_steps = (double*)calloc(w * h, sizeof(double));
        double* success_k = (double*)calloc(w * h, sizeof(double));

        send_to_client(MSG_STATUS, "Simulacia zacala", (uint32_t)strlen("Simulacia zacala"));

        // Hlavny cyklus replikacii
        for (int r = 1; r <= reps; r++) {
            // Kontrola ci simulacia nema skoncit (napr. stop od uzivatela)
            pthread_mutex_lock(&g_sim_lock);
            if (!g_sim_running) {
                pthread_mutex_unlock(&g_sim_lock);
                break;
            }
            pthread_mutex_unlock(&g_sim_lock);

            // Poslanie progresu (kazda 10. replikacia alebo posledna pre znizenie trafficu)
            if (r % 10 == 0 || r == reps) {
                progress_t prog = {(uint32_t)r, (uint32_t)reps};
                send_to_client(MSG_PROGRESS, &prog, sizeof(prog));
            }

            // Pre kazde policko (ktore nie je prekazka) robime pochodzku
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    if (world_get_cell(world, x, y) == CELL_OBSTACLE) continue;

                    // Ak sme v interactive mode, posielame kazdy krok len pre JEDNO policko a JEDNU replikaciu
                    // aby sme nezahltili siet. Podla zadania "vykresluje sa aktualna trajektoria".
                    if (params.mode == 0 && r == 1 && x == w/2 && y == h/2) {
                        int cur_x = x, cur_y = y;
                        int step_cnt = 0;
                        int max_steps = w * h * 5; 
                        
                        while (step_cnt < max_steps && (cur_x != 0 || cur_y != 0)) {
                            interactive_step_t ist = {cur_x, cur_y, (uint32_t)step_cnt};
                            send_to_client(MSG_INTERACTIVE_STEP, &ist, sizeof(ist));
                            
                            simulate_one_step(&cur_x, &cur_y, world, has_obstacles, &probs);
                            step_cnt++;
                            usleep(10000); // spomalenie pre efekt
                        }
                        interactive_step_t ist = {cur_x, cur_y, (uint32_t)step_cnt};
                        send_to_client(MSG_INTERACTIVE_STEP, &ist, sizeof(ist));
                    }

                    // Samotna vypoctova simulacia pre statistiky
                    int steps = simulate_walking(x, y, world, has_obstacles, &probs, 1000000);
                    int idx = y * w + x;
                    total_steps[idx] += steps;
                    if (steps <= (int)K) {
                        success_k[idx] += 1.0;
                    }
                }
            }
        }

        // Vypocet finalnych statistik
        double* avg_steps_final = (double*)malloc(w * h * sizeof(double));
        double* prob_k_final = (double*)malloc(w * h * sizeof(double));

        for (int i = 0; i < w * h; i++) {
            avg_steps_final[i] = total_steps[i] / reps;
            prob_k_final[i] = success_k[i] / reps;

            // Ak je klient v sumarnom mode, posleme mu data
            if (params.mode == 1) {
                summary_cell_t cell;
                cell.x = i % w;
                cell.y = i / w;
                if (params.view == 0) { // avg_steps
                    cell.value_fixed = (int32_t)(avg_steps_final[i] * 100);
                } else { // prob_k
                    cell.value_fixed = (int32_t)(prob_k_final[i] * 10000);
                }
                send_to_client(MSG_SUMMARY_CELL, &cell, sizeof(cell));
            }
        }

        if (params.mode == 0) {
            send_to_client(MSG_INTERACTIVE_DONE, NULL, 0);
        } else {
            send_to_client(MSG_SUMMARY_DONE, NULL, 0);
        }

        // Ulozenie do suboru
        if (strlen(params.save_file) > 0) {
            save_results_to_file(params.save_file, w, h, avg_steps_final, prob_k_final);
            
            // Tiež uložíme svet pre neskorší reload (bod 5)
            char world_save_name[128];
            snprintf(world_save_name, sizeof(world_save_name), "world_%s.bin", params.save_file);
            world_save_to_file(g_world, world_save_name);
            printf("[server] Vysledky ulozene do %s, svet ulozeny do %s\n", params.save_file, world_save_name);
        }

        send_to_client(MSG_STATUS, "Simulacia dokoncena", (uint32_t)strlen("Simulacia dokoncena"));

        free(total_steps);
        free(success_k);
        free(avg_steps_final);
        free(prob_k_final);

        pthread_mutex_lock(&g_sim_lock);
        g_sim_running = 0;
        pthread_mutex_unlock(&g_sim_lock);
    }

    return NULL;
}

int main(int argc, char** argv) {
    srand(time(NULL));
    int port = (argc > 1) ? atoi(argv[1]) : 5555;

    int listen_fd = network_listen_on_port(port, 5);
    if (listen_fd < 0) return 1;

    printf("[server] Server bezi na porte %d\n", port);

    pthread_t sim_tid;
    pthread_create(&sim_tid, NULL, simulation_thread, NULL);

    while (g_running) {
        int client_fd = network_accept_client(listen_fd);
        if (client_fd < 0) continue;

        printf("[server] Klient pripojeny.\n");
        pthread_mutex_lock(&g_client_lock);
        if (g_client_fd >= 0) network_close_fd(&g_client_fd);
        g_client_fd = client_fd;
        pthread_mutex_unlock(&g_client_lock);

        msg_header_t hdr;
        void* payload = NULL;

        while (protocol_receive(client_fd, &hdr, &payload) > 0) {
            switch (hdr.type) {
                case MSG_HELLO:
                    send_to_client(MSG_WELCOME, NULL, 0);
                    break;

                case MSG_START_SIM: {
                    start_sim_t* p = (start_sim_t*)payload;
                    printf("[server] Spustam novu simulaciu: %dx%d, %d replikacii, K=%d\n", 
                           p->world_w, p->world_h, p->replications, p->K);
                    
                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_params = *p;
                    
                    // Vytvorenie sveta
                    if (g_world) world_free(g_world);
                    if (p->world_type == 2) { // zo suboru
                        g_world = world_load_from_file(p->world_file);
                    } else {
                        g_world = world_create(p->world_w, p->world_h);
                        if (p->world_type == 1) { // nahodne prekazky
                            do {
                                world_generate_obstacles(g_world, 0.1);
                            } while (!world_check_reachability(g_world));
                        }
                    }

                    if (g_world) {
                        g_sim_running = 1;
                        pthread_cond_signal(&g_sim_cv);
                        
                        // Poslanie dat o svete klientovi pre vykreslovanie
                        send_to_client(MSG_WORLD_DATA, g_world->grid, g_world->width * g_world->height);
                    } else {
                        send_to_client(MSG_STATUS, "Chyba: Nepodarilo sa vytvorit svet", 33);
                    }
                    pthread_mutex_unlock(&g_sim_lock);
                    break;
                }

                case MSG_RESTART_SIM: {
                    restart_sim_t* r = (restart_sim_t*)payload;
                    printf("[server] Opatovne spustenie simulacie: load=%s, replications=%d, save=%s\n",
                           r->load_file, r->replications, r->save_file);

                    // Pre 62 bodov a "opatovne spustenie uz ukoncenej simulacie" (bod 8 a 11):
                    // "obnovi sa simulacia, ktorej vysledok bol ulozeny do suboru"
                    // Kedze ukladame vysledky (avg_steps, prob_k) do CSV a svet do bin suboru,
                    // a bod 8 hovori o nacitani sveta ("subor, z ktoreho bude simulacia nacitana"),
                    // predpokladajme ze load_file je binarny svet (world_file).
                    
                    pthread_mutex_lock(&g_sim_lock);
                    if (g_world) world_free(g_world);
                    g_world = world_load_from_file(r->load_file);

                    if (g_world) {
                        // Nastavime parametre pre novy beh
                        g_sim_params.world_w = g_world->width;
                        g_sim_params.world_h = g_world->height;
                        g_sim_params.replications = r->replications;
                        g_sim_params.world_type = 2; // from file
                        strncpy(g_sim_params.world_file, r->load_file, 63);
                        strncpy(g_sim_params.save_file, r->save_file, 63);
                        
                        // Ostatne parametre (P_up, K atd.) zostavaju z predchadzajucej simulacie 
                        // alebo by sa mali tiez nacitat. Zadanie bod 8 je trosku nejednoznacne, 
                        // ci sa nacitava len svet alebo komplet stav.
                        
                        g_sim_running = 1;
                        pthread_cond_signal(&g_sim_cv);
                        
                        // Poslanie dat o svete klientovi pre vykreslovanie
                        send_to_client(MSG_WORLD_DATA, g_world->grid, g_world->width * g_world->height);
                        
                        printf("[server] Simulacia obnovena.\n");
                    } else {
                        printf("[server] Chyba: Nepodarilo sa nacitat svet zo suboru %s\n", r->load_file);
                        send_to_client(MSG_STATUS, "Chyba: Nepodarilo sa nacitat svet", 33);
                    }
                    pthread_mutex_unlock(&g_sim_lock);
                    break;
                }

                case MSG_SET_MODE: {
                    set_mode_t* sm = (set_mode_t*)payload;
                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_params.mode = sm->mode;
                    pthread_mutex_unlock(&g_sim_lock);
                    break;
                }

                case MSG_STOP_SIM:
                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_running = 0;
                    pthread_mutex_unlock(&g_sim_lock);
                    break;

                case MSG_BYE:
                    goto disconnect;
            }
            if (payload) { free(payload); payload = NULL; }
        }

disconnect:
        printf("[server] Klient odpojeny.\n");
        if (payload) free(payload);
        pthread_mutex_lock(&g_client_lock);
        network_close_fd(&g_client_fd);
        pthread_mutex_unlock(&g_client_lock);
    }

    network_close_fd(&listen_fd);
    g_running = 0;
    pthread_cond_signal(&g_sim_cv);
    pthread_join(sim_tid, NULL);
    if (g_world) world_free(g_world);

    return 0;
}
