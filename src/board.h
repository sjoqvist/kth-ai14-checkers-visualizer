#ifndef BOARD_H
#define BOARD_H

#include <gtk/gtk.h>

void
draw_board(cairo_t      *cr,
           gint          width_px,
           gint          height_px,
           const gchar  *board,
           const GSList *moves);

#endif /* BOARD_H */
