#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "client_net.h"
#include "protocol.h"
#include "protocol_io.h"
#include "render.h"
#include "menu.h"

static int g_running = 1;
static int g_socket_fd = -1;
static int g_mode = 0; // 0 interactive, 1 summary
static int g_view = 0; // 0 avg, 1 prob

// Vlakno na prijimanie sprav od servera
void* receiver_thread(void* arg) {
    (void)arg;
    msg_header_t hdr;
    void* payload = NULL;

    while (g_running) {
        int fd = g_socket_fd;
        // v pripade ze by sa nepodarilo vytvorit deskriptor soketu
        if (fd < 0) {
            usleep(100000);
            continue;
        }

        int res = protocol_receive(fd, &hdr, &payload);
        if (res <= 0) {
            // Server sa odpojil alebo nieco zlyhalo
            usleep(100000);
            continue;
        }

        // na zaklade typu spravy sa
        switch (hdr.type) {
            case MSG_STATUS:
                printf("\n[server] %s\n", (char*)payload);
                break;
            case MSG_PROGRESS: {
                progress_t* p = (progress_t*)payload;
                printf("\r[progres] %u / %u replikacii", p->repl_index, p->repl_total);
                fflush(stdout);
                if (g_mode == 1) {
                    render_summary_end(g_view);
                }
                if (p->repl_index == p->repl_total) {
                    printf("\n");
                }
                break;
            }
            case MSG_INTERACTIVE_STEP: {
                interactive_step_t* s = (interactive_step_t*)payload;
                render_interactive_step(s->x, s->y);
                break;
            }
            case MSG_WORLD_DATA: {
                render_interactive_world_data((uint8_t*)payload, hdr.payload_len);
                break;
            }
            case MSG_SUMMARY_CELL: {
                summary_cell_t* c = (summary_cell_t*)payload;
                render_summary_cell(c->x, c->y, c->value_fixed);
                break;
            }
            case MSG_SUMMARY_DONE:
                render_summary_end(g_view);
                render_summary_final();
                printf("\n[klient] Hotovo! Stlac ENTER a ideme spat do menu.\n");
                break;
            case MSG_INTERACTIVE_DONE:
                // render_interactive_end(); // Toto by nam zmazalo mapu predcasne
                printf("\n[klient] Replikacia skoncila. Mozete sledovat vykreslovanie dalsej...\n");
                break;
            case MSG_SERVER_EXIT:
                printf("\n[klient] Server process skoncil.\n");
                if (g_socket_fd >= 0) {
                    close(g_socket_fd);
                    g_socket_fd = -1;
                }
                break;
        }
        if (payload) { free(payload); payload = NULL; }
    }
    if (payload) free(payload);
    return NULL;
}

int main(int argc, char** argv) {
    char* ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : 5555;

    pthread_t recv_tid;
    int recv_tid_created = 0;

    while (g_running) {
        menu_print_main();
        int choice = menu_get_choice();

        if (choice == MENU_NEW_SIM || choice == MENU_RESTART_SIM) {
            if (g_socket_fd >= 0) {
                protocol_send(g_socket_fd, MSG_BYE, NULL, 0);
                close(g_socket_fd);
                g_socket_fd = -1;
            }

            pid_t pid = fork();
            if (pid == 0) {
                execl("./server", "./server", "5555", NULL);
                perror("execl server failed");
                exit(1);
            } else if (pid > 0) {
                printf("[client] Zapinam server (pid=%d)...\n", pid);
                
                int connected = 0;
                for (int i = 0; i < 10; i++) {
                    usleep(200000); 
                    g_socket_fd = net_connect_to_server(ip, port);
                    if (g_socket_fd >= 0) {
                        connected = 1;
                        break;
                    }
                }

                if (!connected) {
                    printf("Nepodarilo sa pripojit, server asi nespolupracuje.\n");
                    continue;
                }
                protocol_send(g_socket_fd, MSG_HELLO, NULL, 0);
                
                if (!recv_tid_created) {
                    pthread_create(&recv_tid, NULL, receiver_thread, NULL);
                    recv_tid_created = 1;
                }
            } else {
                perror("fork failed");
            }

            if (choice == MENU_NEW_SIM) {
                start_sim_t params;
                char world_file[64], save_file[64];
                int world_type;
                menu_get_sim_params(&params, world_file, save_file, &world_type);
                
                params.world_type = (uint32_t)world_type;
                strncpy(params.world_file, world_file, 63);
                strncpy(params.save_file, save_file, 63);
                
                g_mode = params.mode;
                g_view = params.view;

                if (g_mode == 0) render_interactive_begin(params.world_w, params.world_h);
                else render_summary_begin(params.world_w, params.world_h, g_view);

                protocol_send(g_socket_fd, MSG_START_SIM, &params, sizeof(params));
            } else {
                restart_sim_t r;
                menu_get_restart_params(r.load_file, &r.replications, r.save_file);
                protocol_send(g_socket_fd, MSG_RESTART_SIM, &r, sizeof(r));
            }
            
            printf("[client] Simulacia bezi, cakam na koniec...\n");
            while (getchar() != '\n'); 
            getchar(); 
            
        } else if (choice == MENU_EXIT) {
            if (g_socket_fd >= 0) {
                protocol_send(g_socket_fd, MSG_BYE, NULL, 0);
            }
            g_running = 0;
            break;
        }
    }

    if (g_socket_fd >= 0) close(g_socket_fd);
    if (recv_tid_created) pthread_join(recv_tid, NULL);
    return 0;
}
