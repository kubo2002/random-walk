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

// Pomocna funkcia na poslanie spravy klientovi
static void send_to_client(uint32_t type, const void* payload, uint32_t payload_len) {
    pthread_mutex_lock(&g_client_lock);
    int fd = g_client_fd;
    pthread_mutex_unlock(&g_client_lock);

    if (fd >= 0) {
        protocol_send(fd, type, payload, payload_len);
    }
}

// Ulozenie vysledkov summary modu do suboru
static void save_results_to_file(const char* filename, int w, int h, double* avg_steps, double* prob_k) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("[server] Nepodarilo sa otvorit subor %s\n", filename);
        return;
    }

    fprintf(f, "X;Y;Priemer_Krokov;Pravdepodobnost_K\n");
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            fprintf(f, "%d;%d;%.2f;%.4f\n", x, y, avg_steps[idx], prob_k[idx]);
        }
    }
    fclose(f);
    printf("[server] Hotovo, vysledky su v: %s\n", filename);
}

// Ulozenie sveta a nastaveni pre restart
static int save_sim_state(const char* filename, world_t* world, start_sim_t* params) {
    FILE* f = fopen(filename, "wb");
    if (!f) return -1;

    fwrite(params, sizeof(start_sim_t), 1, f);
    fwrite(&world->width, sizeof(int), 1, f);
    fwrite(&world->height, sizeof(int), 1, f);
    fwrite(world->grid, sizeof(uint8_t), world->width * world->height, f);

    fclose(f);
    return 0;
}

// Nacitanie sveta a nastaveni pre restart
static int load_sim_state(const char* filename, world_t** world, start_sim_t* params) {
    FILE* f = fopen(filename, "rb");
    if (!f) return -1;

    if (fread(params, sizeof(start_sim_t), 1, f) != 1) { fclose(f); return -1; }

    int w, h;
    if (fread(&w, sizeof(int), 1, f) != 1) { fclose(f); return -1; }
    if (fread(&h, sizeof(int), 1, f) != 1) { fclose(f); return -1; }

    *world = world_create(w, h);
    if (!(*world)) { fclose(f); return -1; }

    if (fread((*world)->grid, sizeof(uint8_t), w * h, f) != (size_t)(w * h)) {
        world_free(*world);
        *world = NULL;
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

// Vlakno pre simulaciu
static void* simulation_thread(void* arg) {
    (void)arg;
    printf("[server] Simulacne vlakno ide.\n");

    while (g_running) {
        // Cakame na start
        pthread_mutex_lock(&g_sim_lock);
        while (g_running && g_sim_running == 0) {
            pthread_cond_wait(&g_sim_cv, &g_sim_lock);
        }
        if (!g_running) {
            pthread_mutex_unlock(&g_sim_lock);
            break;
        }

        // Skopirujeme si nastavenia, nech ich mame lokalne
        start_sim_t params = g_sim_params;
        world_t* world = g_world;
        pthread_mutex_unlock(&g_sim_lock);

        // Odlozime si stav sveta pre restart (bod 5)
        if (strlen(params.save_file) > 0) {
            char bin_save[128];
            snprintf(bin_save, sizeof(bin_save), "%s.state.bin", params.save_file);
            save_sim_state(bin_save, world, &params);
        }

        int w = world->width;
        int h = world->height;
        int reps = params.replications;
        move_probs_t probs = {params.p_up, params.p_down, params.p_left, params.p_right};
        bool has_obstacles = (params.world_type != 0);

        // Polia pre statistiky v summary mode
        double* total_steps = (double*)calloc(w * h, sizeof(double));
        double* success_k = (double*)calloc(w * h, sizeof(double));

        send_to_client(MSG_STATUS, "Simulacia bezi...", (uint32_t)strlen("Simulacia bezi..."));

        // Replikacie
        for (int r = 1; r <= reps; r++) {
            // Mozme to kedykolvek stopnut
            pthread_mutex_lock(&g_sim_lock);
            if (!g_sim_running) {
                pthread_mutex_unlock(&g_sim_lock);
                break;
            }
            pthread_mutex_unlock(&g_sim_lock);

            if (params.mode == 0) {
                // Interaktivny mod - student style
                int cur_x = 0; // Vzdy zacina na (0,0) podla zadania
                int cur_y = 0;
                int max_steps = params.K;
                
                // Resetujeme mapu u klienta na zaciatku kazdej replikacie okrem prvej?
                // Alebo ju nechame vykreslovat trajektorie cez seba? 
                // Zadanie hovori "sa pre kazdu z replikacii bude vykreslovat trajektoria chodca na mape"
                // Ak chceme vidiet vsetky naraz, nerobime reset. Ak chceme kazdu zvlast, robime reset.
                // Student by asi chcel vidiet ako sa to prekryva alebo kazdu nanovo.
                // Skusme poslat specialnu spravu na reset ak by sme chceli, ale v protokole nie je.
                // Vyuzijeme render_interactive_reset v klientovi ak by sme pridali spravu.
                // Alebo proste len pokracujeme.

                for (int step_cnt = 0; step_cnt < max_steps; step_cnt++) {
                    // Posielame suradnice klientovi (relativne k stredu mapy)
                    interactive_step_t ist = {cur_x - (w / 2), cur_y - (h / 2), (uint32_t)step_cnt};
                    send_to_client(MSG_INTERACTIVE_STEP, &ist, sizeof(ist));
                    
                    simulate_one_step(&cur_x, &cur_y, world, has_obstacles, &probs);
                    usleep(50000); // spomalenie aby bolo vidno pohyb
                }
                // Posledny krok
                interactive_step_t ist_f = {cur_x - (w / 2), cur_y - (h / 2), (uint32_t)max_steps};
                send_to_client(MSG_INTERACTIVE_STEP, &ist_f, sizeof(ist_f));
                
                send_to_client(MSG_INTERACTIVE_DONE, NULL, 0);
                usleep(500000);

            } else {
                // Summary mod vypocet - prechadzame kazdu bunku mapy
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        if (world_get_cell(world, x, y) == CELL_OBSTACLE) continue;

                        int steps = simulate_walking(x, y, world, has_obstacles, &probs, params.K);
                        int idx = y * w + x;
                        total_steps[idx] += steps;
                        if (steps <= (int)params.K) {
                            success_k[idx] += 1.0;
                        }
                    }
                }
                
                // Po kazdej replikacii posleme info
                progress_t prog = {(uint32_t)r, (uint32_t)reps};
                send_to_client(MSG_PROGRESS, &prog, sizeof(prog));

                for (int i = 0; i < w * h; i++) {
                    summary_cell_t cell;
                    cell.x = i % w;
                    cell.y = i / w;

                    if (world_get_cell(world, cell.x, cell.y) == CELL_OBSTACLE) {
                        cell.value_fixed = -2;
                        send_to_client(MSG_SUMMARY_CELL, &cell, sizeof(cell));
                    } else {
                        double cur_avg = total_steps[i] / r;
                        cell.value_fixed = (int32_t)(cur_avg * 100);
                        send_to_client(MSG_SUMMARY_CELL, &cell, sizeof(cell));

                        double cur_prob = success_k[i] / r;
                        cell.value_fixed = 2000000 + (int32_t)(cur_prob * 10000);
                        send_to_client(MSG_SUMMARY_CELL, &cell, sizeof(cell));
                    }
                }
            }
        }

        // Koniec simulacie - finalne statistiky
        double* avg_steps_final = (double*)malloc(w * h * sizeof(double));
        double* prob_k_final = (double*)malloc(w * h * sizeof(double));

        for (int i = 0; i < w * h; i++) {
            avg_steps_final[i] = total_steps[i] / reps;
            prob_k_final[i] = success_k[i] / reps;
        }

        if (params.mode == 0) {
            send_to_client(MSG_INTERACTIVE_DONE, NULL, 0);
            printf("[server] Interaktivna simulacia dokoncena.\n");
        } else {
            send_to_client(MSG_SUMMARY_DONE, NULL, 0);
        }

        // Ulozim vysledky
        if (strlen(params.save_file) > 0) {
            save_results_to_file(params.save_file, w, h, avg_steps_final, prob_k_final);
            
            char world_save_name[128];
            snprintf(world_save_name, sizeof(world_save_name), "%s.world.bin", params.save_file);
            world_save_to_file(g_world, world_save_name);
        }

        send_to_client(MSG_STATUS, "Simulacia dokoncena", (uint32_t)strlen("Simulacia dokoncena"));

        free(total_steps);
        free(success_k);
        free(avg_steps_final);
        free(prob_k_final);

        pthread_mutex_lock(&g_sim_lock);
        g_sim_running = 0;
        g_running = 0; // Server vypiname
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

        printf("[server] Niekto sa pripojil.\n");
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
                    printf("[server] Nova simulacia: %dx%d, %d replikacii\n", p->world_w, p->world_h, p->replications);
                    
                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_params = *p;
                    
                    if (g_world) {
                        world_free(g_world);
                        g_world = NULL;
                    }
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
                        
                        // Posleme mapu klientovi
                        send_to_client(MSG_WORLD_DATA, g_world->grid, g_world->width * g_world->height);
                    } else {
                        send_to_client(MSG_STATUS, "Chyba: Svet sa nepodarilo vytvorit", 33);
                    }
                    pthread_mutex_unlock(&g_sim_lock);
                    break;
                }

                case MSG_RESTART_SIM: {
                    restart_sim_t* r = (restart_sim_t*)payload;
                    printf("[server] Restartujeme zo suboru %s\n", r->load_file);
                    
                    pthread_mutex_lock(&g_sim_lock);
                    if (g_world) {
                        world_free(g_world);
                        g_world = NULL;
                    }
                    
                    if (load_sim_state(r->load_file, &g_world, &g_sim_params) == 0) {
                        g_sim_params.replications = r->replications;
                        strncpy(g_sim_params.save_file, r->save_file, 63);
                        
                        g_sim_running = 1;
                        pthread_cond_signal(&g_sim_cv);
                        
                        send_to_client(MSG_WORLD_DATA, g_world->grid, g_world->width * g_world->height);
                    } else {
                        send_to_client(MSG_STATUS, "Chyba: Simulacia sa nenacitala", 29);
                    }
                    pthread_mutex_unlock(&g_sim_lock);
                    break;
                }

                case MSG_BYE:
                    goto disconnect;
            }
            if (payload) { free(payload); payload = NULL; }
        }

disconnect:
        printf("[server] Klient isiel prec.\n");
        if (payload) { free(payload); payload = NULL; }
        pthread_mutex_lock(&g_client_lock);
        network_close_fd(&g_client_fd);
        pthread_mutex_unlock(&g_client_lock);

        pthread_mutex_lock(&g_sim_lock);
        if (!g_sim_running) {
            g_running = 0;
        }
        pthread_mutex_unlock(&g_sim_lock);
    }

    network_close_fd(&listen_fd);
    send_to_client(MSG_SERVER_EXIT, NULL, 0);
    
    g_running = 0;
    pthread_cond_signal(&g_sim_cv);
    pthread_join(sim_tid, NULL);
    if (g_world) {
        world_free(g_world);
        g_world = NULL;
    }

    printf("[server] Server konci, dovidenia.\n");
    return 0;
}
