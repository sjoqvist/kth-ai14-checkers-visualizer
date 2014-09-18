/*
 * board.c
 *
 * Draws the board on a Cairo context, including squares, pieces and moves.
 */
#include <gtk/gtk.h>
#include "board.h"

/* -- macros for colors */
/* square and piece color definitions - borrowed from the problem statement */
#define LIGHT_SQ_R ( 15./ 15.)
#define LIGHT_SQ_G ( 14./ 15.)
#define LIGHT_SQ_B ( 11./ 15.)

#define DARK_SQ_R  (  5./ 15.)
#define DARK_SQ_G  (  8./ 15.)
#define DARK_SQ_B  (  2./ 15.)

#define RED_PL_R   (196./255.)
#define RED_PL_G      0.
#define RED_PL_B   (  3./255.)

#define WHITE_PL_R (255./255.)
#define WHITE_PL_G (249./255.)
#define WHITE_PL_B (244./255.)

/* color for the "move arrow" */
#define MOVE_R        0.
#define MOVE_G        0.
#define MOVE_B        1.

/* color for piece border (including the empty circle for removed pieces) */
#define BORDER_R      0.
#define BORDER_G      0.
#define BORDER_B      0.

/* -- macros for various sizes and distances */
/* font size for the number in the corner of the square */
#define SQUARE_NUMBER_FONTSIZE .35

/* linewidths and circle/mark radii */
#define MOVE_LINEWIDTH         .04
#define PIECE_CIRCLE_LINEWIDTH .03
#define KING_MARK_LINEWIDTH    .05
#define PIECE_CIRCLE_RADIUS    .3
#define KING_MARK_RADIUS       .15

/* -- macros to get the row and column for a dark square in the range 0..31 */
/* the first one's simple */
#define BOARD_ROW(i) ((i)/4)
/* this is more complicated: use the three least significant bits, rotate them
 * to the left and flip the least significant bit, so that
 * [b_4, b_3, b_2, b_1, b_0] becomes [0, 0, b_1, b_0, !b_2], or in other words
 * (given that rows and columns are both in the range 0..7) map
 *  xx000 => 00001  (even row, column 1)
 *  xx001 => 00011  (even row, column 3)
 *  xx010 => 00101  (even row, column 5)
 *  xx011 => 00111  (even row, column 7)
 *  xx100 => 00000  ( odd row, column 0)
 *  xx101 => 00010  ( odd row, column 2)
 *  xx110 => 00100  ( odd row, column 4)
 *  xx111 => 00110  ( odd row, column 6) */
#define BOARD_COL(i) ((((i)&3)<<1) | (((i)&4)==0))

/* -- macros meant to make the code easier to read */
/* "for" loop limit when iterating over the dark squares in the board */
#define NUM_DARK_SQ 32

/* floating point arithmetic isn't commutative, so to allow the compiler to
   pre-calculate constants if we're planning to draw off-center, SQ_CENTER
   should be placed together with the other constant _within parentheses_ */
#define SQ_CENTER .5

/* CENTER is to be used iff we're not adding another constant afterwards */
#define CENTER(x) ((x)+SQ_CENTER)

/*
 * Function: draw_pieces
 * ---------------------
 * Draws piece circles, but doesn't fill or stroke.
 *
 * cr:     a Cairo context
 * board:  a string where each character represents a dark square, and
 *         where a lowercase/uppercase letter represents a man/king
 * player: uppercase 'R' or 'W'
 */
static void
draw_pieces(cairo_t * const cr, const gchar * const board, const gchar player)
{
  int i;
  for (i=0; i<NUM_DARK_SQ; ++i) {
    /* 'A'-'Z' are in ASCII range 0x41-0x5a and 'a'-'z' in range 0x61-0x7a
       hence, we can check for either by masking out 0x20 */
    if ((board[i] & ~0x20) == player) {
      const int x = BOARD_COL(i);
      const int y = BOARD_ROW(i);
      cairo_move_to(cr, x+(SQ_CENTER+PIECE_CIRCLE_RADIUS), CENTER(y));
      cairo_arc(cr, CENTER(x), CENTER(y), PIECE_CIRCLE_RADIUS, 0., 2*G_PI);
    }
  }
}

/*
 * Function: draw_king_markers
 * ---------------------------
 * Draws markers on pieces which are kings, but doesn't stroke.
 *
 * cr:     a Cairo context
 * board:  a string where each character represents a dark square, and
 *         where uppercase letters represents kings
 */
static void
draw_king_markers(cairo_t * const cr, const gchar * const board)
{
  int i;
  for (i=0; i<NUM_DARK_SQ; ++i) {
    if (board[i] >= 'A' && board[i] <= 'Z') {
      const int x = BOARD_COL(i);
      const int y = BOARD_ROW(i);
      cairo_move_to(cr, x+(SQ_CENTER-KING_MARK_RADIUS), CENTER(y));
      cairo_line_to(cr, x+(SQ_CENTER+KING_MARK_RADIUS), CENTER(y));
      cairo_move_to(cr, CENTER(x), y+(SQ_CENTER-KING_MARK_RADIUS));
      cairo_line_to(cr, CENTER(x), y+(SQ_CENTER+KING_MARK_RADIUS));
    }
  }
}

/*
 * Function: draw_moves
 * --------------------
 * Draws a line along the path of a move or jump, but doesn't stroke.
 *
 * cr:     a Cairo context
 * moves:  a singly-linked list of dark squares (range 0..31) involved in a
 *         sequence of movements (may also be empty or contain one element)
 */
static void
draw_moves(cairo_t * const cr, const GSList * const moves)
{
  const GSList *move = moves;

  while (move != NULL) {
    const int sq = GPOINTER_TO_INT(move->data);
    const int y  = BOARD_ROW(sq);
    const int x  = BOARD_COL(sq);
    /* only move if it's the first square in the sequence */
    if (move == moves) {
      cairo_move_to(cr, CENTER(x), CENTER(y));
    } else {
      cairo_line_to(cr, CENTER(x), CENTER(y));
    }
    move = g_slist_next(move);
  }
}

/*
 * Function: draw_board
 * --------------------
 * Draws the board with squares, numbers, moves and pieces (men and kings)
 * including removed pieces.
 *
 * cr:        the Cairo context to draw on
 * width_px:  the width of the widget in pixels
 * height_px: the height of the widget in pixels
 * board:     a string where the first 32 characters represent the content of
 *            the dark squares, according to the following list (or NULL)
 *              * 'r' - red man
 *              * 'R' - red king
 *              * 'w' - white man
 *              * 'W' - white king
 *              * 'x' - recently removed piece
 *              * '.' - empty square
 * moves:     a singly-linked list with a sequence of moves between the dark
 *            squares (range 0..31) in order (or NULL)
 */
void
draw_board(cairo_t      * const cr,
           const gint           width_px,
           const gint           height_px,
           const gchar  * const board,
           const GSList * const moves)
{
  /* scale the drawing area to (0,0) -- (8,8) */
  cairo_scale(cr, width_px/8., height_px/8.);

  /* draw light background */
  cairo_rectangle(cr, 0., 0., 8., 8.);
  cairo_set_source_rgb(cr, LIGHT_SQ_R, LIGHT_SQ_G, LIGHT_SQ_B);
  cairo_fill(cr);

  /* draw dark squares */
  {
    int i;
    for (i=0; i<NUM_DARK_SQ; ++i) {
      cairo_rectangle(cr, BOARD_COL(i), BOARD_ROW(i), 1., 1.);
    }
  }
  cairo_set_source_rgb(cr, DARK_SQ_R, DARK_SQ_G, DARK_SQ_B);
  cairo_fill(cr);

  /* draw square numbers */
  cairo_set_font_size(cr, SQUARE_NUMBER_FONTSIZE);
  cairo_set_source_rgb(cr, LIGHT_SQ_R, LIGHT_SQ_G, LIGHT_SQ_B);
  {
    static const gchar * const square_num[] =
      { "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",
        "9",  "10", "11", "12", "13", "14", "15", "16",
        "17", "18", "19", "20", "21", "22", "23", "24",
        "25", "26", "27", "28", "29", "30", "31", "32" };
    int i = 0;
    for (i=0; i<NUM_DARK_SQ; ++i) {
      cairo_move_to(cr, BOARD_COL(i), BOARD_ROW(i)+1);
      cairo_show_text(cr, square_num[i]);
    }
  }

  /* return if there are no pieces to draw */
  if (board == NULL) return;

  /* draw removed pieces - before movement lines (place below) */
  draw_pieces(cr, board, 'X');
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_set_line_width(cr, PIECE_CIRCLE_LINEWIDTH);
  cairo_stroke(cr);

  /* draw moves - before remaining pieces (place below) */
  draw_moves(cr, moves);
  cairo_set_source_rgb(cr, MOVE_R, MOVE_G, MOVE_B);
  cairo_set_line_width(cr, MOVE_LINEWIDTH);
  cairo_stroke(cr);

  /* set linewidth for both colors of pieces */
  cairo_set_line_width(cr, PIECE_CIRCLE_LINEWIDTH);

  /* draw red pieces */
  draw_pieces(cr, board, 'R');
  cairo_set_source_rgb(cr, RED_PL_R, RED_PL_G, RED_PL_B);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_stroke(cr);

  /* draw white pieces */
  draw_pieces(cr, board, 'W');
  cairo_set_source_rgb(cr, WHITE_PL_R, WHITE_PL_G, WHITE_PL_B);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_stroke(cr);

  /* draw king markers - after remaining pieces (place above) */
  cairo_set_line_width(cr, KING_MARK_LINEWIDTH);
  draw_king_markers(cr, board);
  cairo_stroke(cr);
}
