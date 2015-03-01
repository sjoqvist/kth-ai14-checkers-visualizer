/*!
 * \file board.c
 * \brief
 * Draws the board on a Cairo context, including squares, pieces and moves.
 */
#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>
#include "board.h"
#include "gui.h"
#include "main.h"

/* -- macros for colors */
/* square and piece color definitions - borrowed from the problem statement */
#define LIGHT_SQ_R ( 15./ 15.) /*!< \brief Light square, red component */
#define LIGHT_SQ_G ( 14./ 15.) /*!< \brief Light square, green component */
#define LIGHT_SQ_B ( 11./ 15.) /*!< \brief Light square, blue component */

#define DARK_SQ_R  (  5./ 15.) /*!< \brief Dark square, red component */
#define DARK_SQ_G  (  8./ 15.) /*!< \brief Dark square, green component */
#define DARK_SQ_B  (  2./ 15.) /*!< \brief Dark square, blue component */

#define RED_PL_R   (196./255.) /*!< \brief Red player, red component */
#define RED_PL_G   (  0./255.) /*!< \brief Red player, green component */
#define RED_PL_B   (  3./255.) /*!< \brief Red player, blue component */

#define WHITE_PL_R (255./255.) /*!< \brief White player, red component */
#define WHITE_PL_G (249./255.) /*!< \brief White player, green component */
#define WHITE_PL_B (244./255.) /*!< \brief White player, blue component */

/* color for the "move arrow" */
#define MOVE_R     (  0./255.) /*!< \brief Move arrow, red component */
#define MOVE_G     (  0./255.) /*!< \brief Move arrow, green component */
#define MOVE_B     (255./255.) /*!< \brief Move arrow, blue component */

/* color for piece border (including the empty circle for removed pieces) */
#define BORDER_R   (  0./255.) /*!< \brief Piece border, red component */
#define BORDER_G   (  0./255.) /*!< \brief Piece border, green component */
#define BORDER_B   (  0./255.) /*!< \brief Piece border, blue component */

/* -- macros for various sizes and distances */
/*!
 * \brief
 * Font size for the number in the corner of the square.
 */
#define SQUARE_NUMBER_FONTSIZE .35

/* linewidths and circle/mark radii */
#define MOVE_LINEWIDTH         .04 /*!< \brief Move arrow linewidth */
#define PIECE_BORDER_LINEWIDTH .03 /*!< \brief Piece border linewidth */
#define KING_MARK_LINEWIDTH    .05 /*!< \brief King mark linewidth */
#define PIECE_RADIUS           .3  /*!< \brief Piece circle radius */
#define KING_MARK_RADIUS       .15 /*!< \brief King mark size */

/* -- macros to get the row and column for a dark square in the range 0..31 */
/*!
 * \brief
 * Gets the board row from a dark square index
 */
#define BOARD_ROW(i) ((i)/4)
/*!
 * \brief
 * Gets the board column from a dark square index
 *
 * Use the three least significant bits, rotate them to the left and flip the
 * least significant bit, so that \f$[b_4, b_3, b_2, b_1, b_0]\f$ becomes
 * \f$[0, 0, b_1, b_0, !b_2]\f$, or in other words
 * (given that rows and columns are both in the range `0..7`) map
 *
 * From (square id) | To (column id) | Row  | Column
 * ---------------- | -------------- | ---: | ------
 * xx000            | 00001          | even | 1
 * xx001            | 00011          | even | 3
 * xx010            | 00101          | even | 5
 * xx011            | 00111          | even | 7
 * xx100            | 00000          |  odd | 0
 * xx101            | 00010          |  odd | 2
 * xx110            | 00100          |  odd | 4
 * xx111            | 00110          |  odd | 6
 */
#define BOARD_COL(i) ((((i)&3)<<1) | (((i)&4)==0))

/* -- macros meant to make the code easier to read */
/*!
 * \brief
 * Offset to reach the center of a square on the board
 *
 * \attention
 * Floating point arithmetic isn't commutative, so to allow the compiler to
 * pre-calculate constants if you're planning to draw off-center, #SQ_CENTER
 * should be placed together with the other constant _within parentheses_.
 */
#define SQ_CENTER .5

/*!
 * \brief
 * Converts a coordinate from a square corner to a square center
 *
 * \note
 * #CENTER(x) is to be used iff you want to refer to the exact center of a
 * square, and not combine it with another constant. Otherwise, you should use
 * #SQ_CENTER to allow the compiler to pre-calculate the constants.
 */
#define CENTER(x) ((x)+SQ_CENTER)

/*!
 * \brief
 * Draws piece circles, but doesn't fill or stroke.
 *
 * \param[in] cr     a Cairo context
 * \param[in]        board a string where each character represents a dark
 *                   square, and where a lowercase/uppercase letter represents
 *                   a man/king
 * \param[in] player uppercase \c R (red), \c W (white) or \c X (removed)
 */
static void
draw_pieces(cairo_t * const cr, const gchar * const board, const gchar player)
{
  assert(cr != NULL);
  assert(board != NULL);
  assert(strlen(board) == NUM_DARK_SQ);
  assert(player == 'R' || player == 'W' || player == 'X');

  for (guint8 i=0; i<NUM_DARK_SQ; ++i) {
    /* 'A'-'Z' are in ASCII range 0x41-0x5a and 'a'-'z' in range 0x61-0x7a
       hence, we can check for either by masking out 0x20 */
    if ((board[i] & ~0x20) == player) {
      const guint8 x = BOARD_COL(i);
      const guint8 y = BOARD_ROW(i);
      cairo_move_to(cr, x+(SQ_CENTER+PIECE_RADIUS), CENTER(y));
      cairo_arc(cr, CENTER(x), CENTER(y), PIECE_RADIUS, 0., 2*G_PI);
    }
  }
}

/*!
 * \brief
 * Draws markers on pieces which are kings, but doesn't stroke.
 *
 * \param[in] cr    a Cairo context
 * \param[in] board a string where each character represents a dark square,
 *                  and where uppercase letters represent kings
 */
static void
draw_king_markers(cairo_t * const cr, const gchar * const board)
{
  assert(cr != NULL);
  assert(board != NULL);
  assert(strlen(board) == NUM_DARK_SQ);

  for (guint8 i=0; i<NUM_DARK_SQ; ++i) {
    if (board[i] >= 'A' && board[i] <= 'Z') {
      const guint8 x = BOARD_COL(i);
      const guint8 y = BOARD_ROW(i);
      cairo_move_to(cr, x+(SQ_CENTER-KING_MARK_RADIUS), CENTER(y));
      cairo_line_to(cr, x+(SQ_CENTER+KING_MARK_RADIUS), CENTER(y));
      cairo_move_to(cr, CENTER(x), y+(SQ_CENTER-KING_MARK_RADIUS));
      cairo_line_to(cr, CENTER(x), y+(SQ_CENTER+KING_MARK_RADIUS));
    }
  }
}

/*!
 * \brief
 * Draws a line along the path of a move or jump, but doesn't stroke.
 *
 * \param[in] cr    a Cairo context
 * \param[in] moves a singly-linked list of dark squares (range `0..31`)
 *                  involved in a sequence of movements (may be empty or
 *                  contain one element, but that causes nothing to be drawn)
 */
static void
draw_moves(cairo_t * const cr, const GSList * const moves)
{
  const GSList *move = moves;

  assert(cr != NULL);

  while (move != NULL) {
    const guint8 sq = GPOINTER_TO_UINT(move->data);
    const guint8 y  = BOARD_ROW(sq);
    const guint8 x  = BOARD_COL(sq);

    assert(sq < NUM_DARK_SQ);

    /* only move if it's the first square in the sequence */
    if (move == moves) {
      cairo_move_to(cr, CENTER(x), CENTER(y));
    } else {
      cairo_line_to(cr, CENTER(x), CENTER(y));
    }
    move = g_slist_next(move);
  }
}

/* documented in board.h */
void
draw_board(cairo_t      * const cr,
           const int            width_px,
           const int            height_px,
           const gchar  * const board,
           const GSList * const moves)
{
  assert(cr != NULL);
  assert(board == NULL ||
         g_regex_match_simple("^[rRwWx.]{"STR(NUM_DARK_SQ)"}$", board, 0, 0));

  /* scale the drawing area to (0,0) -- (8,8) */
  cairo_scale(cr, width_px/8., height_px/8.);

  /* draw light background */
  cairo_rectangle(cr, 0., 0., 8., 8.);
  cairo_set_source_rgb(cr, LIGHT_SQ_R, LIGHT_SQ_G, LIGHT_SQ_B);
  cairo_fill(cr);

  /* draw dark squares */
  for (guint8 i=0; i<NUM_DARK_SQ; ++i) {
    cairo_rectangle(cr, BOARD_COL(i), BOARD_ROW(i), 1., 1.);
  }
  cairo_set_source_rgb(cr, DARK_SQ_R, DARK_SQ_G, DARK_SQ_B);
  cairo_fill(cr);

  /* draw square numbers */
  cairo_set_font_size(cr, SQUARE_NUMBER_FONTSIZE);
  cairo_set_source_rgb(cr, LIGHT_SQ_R, LIGHT_SQ_G, LIGHT_SQ_B);
  {
    static const gchar * const square_num[NUM_DARK_SQ] =
      { "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",
        "9",  "10", "11", "12", "13", "14", "15", "16",
        "17", "18", "19", "20", "21", "22", "23", "24",
        "25", "26", "27", "28", "29", "30", "31", "32" };
    for (guint8 i=0; i<NUM_DARK_SQ; ++i) {
      cairo_move_to(cr, BOARD_COL(i), BOARD_ROW(i)+1);
      cairo_show_text(cr, square_num[i]);
    }
  }

  /* return if there are no pieces to draw */
  if (board == NULL) return;

  /* draw removed pieces - before movement lines (place below) */
  draw_pieces(cr, board, 'X');
  cairo_set_source_rgb(cr, BORDER_R, BORDER_G, BORDER_B);
  cairo_set_line_width(cr, PIECE_BORDER_LINEWIDTH);
  cairo_stroke(cr);

  /* draw moves - before remaining pieces (place below) */
  draw_moves(cr, moves);
  cairo_set_source_rgb(cr, MOVE_R, MOVE_G, MOVE_B);
  cairo_set_line_width(cr, MOVE_LINEWIDTH);
  cairo_stroke(cr);

  /* set linewidth for both colors of pieces */
  cairo_set_line_width(cr, PIECE_BORDER_LINEWIDTH);

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
