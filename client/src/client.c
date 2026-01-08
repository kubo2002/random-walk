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

// Vlakno na prijimanie sprav zo servera
void* receiver_thread(void* arg) {
    (void)arg;
    msg_header_t hdr;
    void* payload = NULL;

    while (g_running && protocol_receive(g_socket_fd, &hdr, &payload) > 0) {
        switch (hdr.type) {
            case MSG_STATUS:
                printf("\n[server] %s\n", (char*)payload);
                break;
            case MSG_PROGRESS: {
                progress_t* p = (progress_t*)payload;
                printf("\r[progres] %u / %u replikacii", p->repl_index, p->repl_total);
                fflush(stdout);
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
                printf("\n[klient] Simulacia dokoncena. Stlacte ENTER pre navrat do menu.\n");
                break;
            case MSG_INTERACTIVE_DONE:
                render_interactive_end();
                printf("\n[klient] Interaktivna simulacia dokoncena. Stlacte ENTER pre navrat do menu.\n");
                break;
        }
        if (payload) { free(payload); payload = NULL; }
    }
    g_running = 0;
    return NULL;
}

int main(int argc, char** argv) {
    char* ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : 5555;

    g_socket_fd = net_connect_to_server(ip, port);
    if (g_socket_fd < 0) {
        printf("Nepodarilo sa pripojit k serveru.\n");
        return 1;
    }

    protocol_send(g_socket_fd, MSG_HELLO, NULL, 0);

    pthread_t recv_tid;
    pthread_create(&recv_tid, NULL, receiver_thread, NULL);

    while (g_running) {
        menu_print_main();
        int choice = menu_get_choice();

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
            
            // Cakanie na koniec simulacie, aby menu neprekrylo vystup
            printf("[client] Simulacia bezi, pockajte na dokoncenie...\n");
            
            // Po skonceni simulacie (ked receiver thread vypise spravu),
            // chceme aby si user mohol v klude pozriet vysledok.
            // Preto tu pridame getchar() na "ENTER" pre navrat.
            while (getchar() != '\n'); // vycisti buffer
            getchar(); // pockaj na enter
            
        } else if (choice == MENU_JOIN_SIM) {
            printf("Pripajanie k beziacej simulacii...\n");
            // Server automaticky posle data novemu klientovi
        } else if (choice == MENU_RESTART_SIM) {
            restart_sim_t r;
            menu_get_restart_params(r.load_file, &r.replications, r.save_file);
            protocol_send(g_socket_fd, MSG_RESTART_SIM, &r, sizeof(r));
            
            printf("[client] Restart simulacie bezi, pockajte na dokoncenie...\n");
            while (getchar() != '\n'); 
            getchar(); 

        } else if (choice == MENU_EXIT) {
            protocol_send(g_socket_fd, MSG_BYE, NULL, 0);
            g_running = 0;
            break;
        } else if (choice == 5) { // Skryta volba pre zmenu zobrazenia
            g_view = 1 - g_view;
            set_view_t sv = {(uint32_t)g_view};
            protocol_send(g_socket_fd, MSG_SET_VIEW, &sv, sizeof(sv));
            printf("Zobrazenie zmenene na: %s\n", g_view == 0 ? "AvgSteps" : "ProbK");
        }
    }

    close(g_socket_fd);
    pthread_join(recv_tid, NULL);
    return 0;
}
