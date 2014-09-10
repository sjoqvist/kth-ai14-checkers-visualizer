#include <gtk/gtk.h>
#include "board.h"

#define LIGHT_SQ_R (15./15.)
#define LIGHT_SQ_G (14./15.)
#define LIGHT_SQ_B (11./15.)

#define DARK_SQ_R (5./15.)
#define DARK_SQ_G (8./15.)
#define DARK_SQ_B (2./15.)

#define RED_PL_R (196./255.)
#define RED_PL_G 0.
#define RED_PL_B (3./255.)

#define WHITE_PL_R (255./255.)
#define WHITE_PL_G (249./255.)
#define WHITE_PL_B (244./255.)

#define MOVE_R 0.
#define MOVE_G 0.
#define MOVE_B 1.

#define BORDER_R 0.
#define BORDER_G 0.
#define BORDER_B 0.

#define MOVE_LINEWIDTH .04
#define PIECE_CIRCLE_LINEWIDTH .03
#define KING_MARK_LINEWIDTH .05
#define PIECE_CIRCLE_RADIUS .3
#define KING_MARK_RADIUS .15

/* appearance of the board - used for communication with other parts
   of the code */
gchar  *str_pieces = NULL;
GSList *list_moves = NULL;

static void
draw_pieces(cairo_t *cr, gchar player)
{
  int i;
  for (i=0; i<32; ++i) {
    if (str_pieces[i] == player || str_pieces[i] == player + ('a'-'A')) {
      const int y = i/4;
      const int x = (i%4)*2+1-(y&1);
      cairo_move_to(cr, x+.8, y+.5);
      cairo_arc(cr, x+.5, y+.5, PIECE_CIRCLE_RADIUS, 0, 2*G_PI);
    }
  }
}

static void
draw_king_markers(cairo_t *cr)
{
  int i;
  for (i=0; i<32; ++i) {
    if (str_pieces[i] >= 'A' && str_pieces[i] <= 'Z') {
      const int y = i/4;
      const int x = (i%4)*2+1-(y&1);
      cairo_move_to(cr, x+.5-KING_MARK_RADIUS, y+.5);
      cairo_line_to(cr, x+.5+KING_MARK_RADIUS, y+.5);
      cairo_move_to(cr, x+.5, y+.5-KING_MARK_RADIUS);
      cairo_line_to(cr, x+.5, y+.5+KING_MARK_RADIUS);
    }
  }
}

static void
draw_moves(cairo_t *cr)
{
  GSList *move = list_moves;

  while (move != NULL) {
    gint x;
    gint y;
    gint pos;
    pos = GPOINTER_TO_INT(move->data);
    y = pos/4;
    x = (pos%4)*2+1-(y&1);
    if (move == list_moves) {
      cairo_move_to(cr, x+.5, y+.5);
    } else {
      cairo_line_to(cr, x+.5, y+.5);
    }
    move = g_slist_next(move);
  }
}

gboolean
draw_board(GtkWidget *widget)
{
  cairo_t *cr;

  cr = gdk_cairo_create(widget->window);
  cairo_scale(cr, widget->allocation.width/8., widget->allocation.height/8.);

  /* draw light background */
  cairo_rectangle(cr, 0, 0, 8, 8);
  cairo_set_source_rgb(cr, LIGHT_SQ_R, LIGHT_SQ_G, LIGHT_SQ_B);
  cairo_fill(cr);

  /* draw dark squares */
  {
    int y;
    for (y=0; y<8; ++y) {
      int x;
      for (x=1-(y&1); x<8; x+=2) {
        cairo_rectangle(cr, x, y, 1, 1);
      }
    }
  }
  cairo_set_source_rgb(cr, DARK_SQ_R, DARK_SQ_G, DARK_SQ_B);
  cairo_fill(cr);

  /* numbers in the cells */
  {
    gchar cell_num[8];
    int id = 0;
    int y;
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, .35);
    cairo_set_source_rgb(cr, LIGHT_SQ_R, LIGHT_SQ_G, LIGHT_SQ_B);
    for (y=0; y<8; ++y) {
      int x;
      for (x=1-(y&1); x<8; x+=2) {
        g_snprintf(cell_num, sizeof(cell_num), "%d", ++id);
        cairo_move_to(cr, x, y+1);
        cairo_show_text(cr, cell_num);
      }
    }
  }

  /* return if there are no pieces to draw */
  if (str_pieces == NULL) {
    cairo_destroy(cr);
    return TRUE;
  }

  /* removed pieces */
  draw_pieces(cr, 'X');
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_set_line_width(cr, PIECE_CIRCLE_LINEWIDTH);
  cairo_stroke(cr);

  /* moves */
  draw_moves(cr);
  cairo_set_source_rgb(cr, MOVE_R, MOVE_G, MOVE_B);
  cairo_set_line_width(cr, MOVE_LINEWIDTH);
  cairo_stroke(cr);

  cairo_set_line_width(cr, PIECE_CIRCLE_LINEWIDTH);
  /* red player */
  draw_pieces(cr, 'R');
  cairo_set_source_rgb(cr, RED_PL_R, RED_PL_G, RED_PL_B);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_stroke(cr);

  /* white player */
  draw_pieces(cr, 'W');
  cairo_set_source_rgb(cr, WHITE_PL_R, WHITE_PL_G, WHITE_PL_B);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_stroke(cr);

  /* draw king markers */
  cairo_set_line_width(cr, KING_MARK_LINEWIDTH);
  draw_king_markers(cr);
  cairo_stroke(cr);

  cairo_destroy(cr);
  return TRUE;
}
