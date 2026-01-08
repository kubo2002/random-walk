#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

/*
    Render modul pre "summary mode".
    Ulozi si maticu a po SUMMARY_DONE ju vypise.

    view:
      0 = avg_steps (value_fixed je avg*100)
      1 = prob_k    (value_fixed je prob*10000)
*/

void render_summary_begin(int w, int h, int view);
void render_summary_cell(int x, int y, int value_fixed);
void render_summary_end(int view);

void render_interactive_begin(int w, int h);
void render_interactive_world_data(const uint8_t* data, uint32_t len);
void render_interactive_step(int x, int y);
void render_interactive_end(void);
void render_interactive_reset(void);
#endif
