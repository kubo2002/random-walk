#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
    JEDNODUCHY BINARY PROTOCOL (client <-> server)

    Kazda sprava:
    [msg_header_t][payload bytes...]

    Header ma fixnu velkost.
    payload_len povie kolko bajtov nasleduje za headerom.

    Poznamka:
    - Pre jednoduchost posielame cisla v network byte order (htonl/ntohl).
    - Pouzivame uint32_t pre vacsinu poli.
    - VELA VECI SA TU MOZE ESTE POMENIT
*/
typedef enum {
    // client -> server
    MSG_HELLO = 1,          // "som tu", verzia protokolu
    MSG_START_SIM = 2,      // start simulacie (parametre)
    MSG_SET_MODE = 3,       // zmena modu (interactive/summary)
    MSG_SET_VIEW = 4,       // v summary mode: AVG_STEPS alebo PROB_K
    MSG_STOP_SIM = 5,       // ukonci simulaciu (server skonci simulaciu)
    MSG_BYE = 6,            // klient ide prec

    // server -> client
    MSG_WELCOME = 100,      // odpoved na HELLO
    MSG_STATUS = 101,       // textovy status (napr. "started", "error")
    MSG_PROGRESS = 102,     // repl_index / repl_total
    MSG_INTERACTIVE_STEP = 103, // pozicia chodca (len pre interactive)
    MSG_SUMMARY_CELL = 104, // jedna bunka summary (x,y,value)
    MSG_SUMMARY_DONE = 105  // summary poslane cele
} msg_type_t;

/*
    Header spravy.
    type = typ spravy
    payload_len = kolko bajtov ide za headerom
*/
typedef struct {
    uint32_t type;
    uint32_t payload_len;
} msg_header_t;


/* ---------------- payload struktury ---------------- */

/*
    HELLO: klient posle verziu protokolu (1)
*/
typedef struct {
    uint32_t version;
} hello_t;

/*
    START_SIM: parametre simulacie (minimalny set)
    - world_w, world_h: rozmery sveta
    - replications: kolko replikacii
    - K: max krokov pre pravdepodobnost
    - p_up, p_down, p_left, p_right: pravdepodobnosti v promile (0..1000), suma musi byt 1000
    - mode: 0=interactive, 1=summary
    - view: 0=avg_steps, 1=prob_k  (relevantne hlavne v summary mode)
*/
typedef struct {
    uint32_t world_w;
    uint32_t world_h;
    uint32_t replications;
    uint32_t K;

    uint32_t p_up;
    uint32_t p_down;
    uint32_t p_left;
    uint32_t p_right;

    uint32_t mode;
    uint32_t view;
} start_sim_t;

/*
    SET_MODE: iba prepnutie modu
*/
typedef struct {
    uint32_t mode; // 0=interactive, 1=summary
} set_mode_t;

/*
    SET_VIEW: v summary mode si klient moze prepnut zobrazenie
*/
typedef struct {
    uint32_t view; // 0=avg_steps, 1=prob_k
} set_view_t;

/*
    PROGRESS: server posiela priebezny stav replikacii
*/
typedef struct {
    uint32_t repl_index; // od 1
    uint32_t repl_total;
} progress_t;

/*
    INTERACTIVE_STEP: server posiela aktualnu poziciu chodca
    Pre jednoduchost: x,y ako signed int32 (mozu byt aj zaporne, lebo mame stred [0,0])
*/
typedef struct {
    int32_t x;
    int32_t y;
} interactive_step_t;

/*
    SUMMARY_CELL: jedna bunka tabulky (x,y,value)
    value:
    - ak view=avg_steps: priemer krokov (napr. 12.34) -> posleme ako fixed point *100
    - ak view=prob_k: pravdepodobnost (napr. 0.56) -> posleme ako fixed point *10000
*/
typedef struct {
    int32_t x;
    int32_t y;
    int32_t value_fixed;
} summary_cell_t;

#endif
