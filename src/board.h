/*!
 * \file board.h
 * \brief
 * Provides functions to visually draw a board representation on screen
 */
#ifndef BOARD_H
#define BOARD_H

#include <gtk/gtk.h>

/*!
 * \brief
 * Draws the board with squares, numbers, moves and pieces (men and kings)
 * including removed pieces.
 *
 * \param[in] cr        the Cairo context to draw on
 * \param[in] width_px  the width of the widget in pixels
 * \param[in] height_px the height of the widget in pixels
 * \param[in] board
 * \parblock
 * a string that represents the content of each of the dark squares, according
 * to the following table (or \c NULL to draw an empty board)
 *
 * Character | Meaning
 * --------- | -------
 * \c r      | red man
 * \c R      | red king
 * \c w      | white man
 * \c W      | white king
 * \c x      | recently removed piece
 * \c .      | empty square
 * \endparblock
 * \param[in] moves     a singly-linked list with a sequence of moves between
 *                      the dark squares (range `0..31`) in order (or \c NULL)
 */
void
draw_board(cairo_t      *cr,
           int           width_px,
           int           height_px,
           const gchar  *board,
           const GSList *moves);

#endif /* BOARD_H */
