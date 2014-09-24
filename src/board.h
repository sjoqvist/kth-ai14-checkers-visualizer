#ifndef BOARD_H
#define BOARD_H

#include <gtk/gtk.h>

void
draw_board(cairo_t      *cr,
           int           width_px,
           int           height_px,
           const gchar  *board,
           const GSList *moves);

#endif /* BOARD_H */
