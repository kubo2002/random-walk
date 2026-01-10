#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Typy správ, čo si posielame
typedef enum {
    MSG_HELLO = 1,          // čau server
    MSG_START_SIM = 2,      // spusti simuláciu
    MSG_BYE = 6,            // končím
    MSG_RESTART_SIM = 7,    // načítaj zo súboru

    MSG_WELCOME = 100,      // vitaj klient
    MSG_STATUS = 101,       // nejaký text
    MSG_PROGRESS = 102,     // koľko replikácií už je
    MSG_INTERACTIVE_STEP = 220, // kde je chodec
    MSG_INTERACTIVE_DONE = 222, // chodec došiel
    MSG_WORLD_DATA = 230,   // mapa s prekážkami
    MSG_SUMMARY_CELL = 104, // dáta pre jednu bunku tabuľky
    MSG_SUMMARY_DONE = 105, // tabuľky sú hotové
    MSG_SERVER_EXIT = 106   // server sa vypína
} msg_type_t;

// Hlavička každej správy
typedef struct {
    uint32_t type;
    uint32_t payload_len;
} msg_header_t;

// Čo posielame pri štarte
typedef struct {
    uint32_t world_w;
    uint32_t world_h;
    uint32_t replications;
    uint32_t K;
    uint32_t p_up;
    uint32_t p_down;
    uint32_t p_left;
    uint32_t p_right;
    uint32_t mode; // 0 interactive, 1 summary
    uint32_t view; 
    uint32_t world_type;
    char world_file[64];
    char save_file[64];
    int32_t start_x;
    int32_t start_y;
} start_sim_t;

// Pre restart
typedef struct {
    uint32_t replications;
    char load_file[64];
    char save_file[64];
} restart_sim_t;

// Priebeh
typedef struct {
    uint32_t repl_index;
    uint32_t repl_total;
} progress_t;

// Krok chodca
typedef struct {
    int32_t x;
    int32_t y;
    uint32_t step;
} interactive_step_t;

// Bunka tabuľky
typedef struct {
    int32_t x;
    int32_t y;
    int32_t value_fixed;
} summary_cell_t;

#endif
