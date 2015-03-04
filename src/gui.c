/*!
 * \file gui.c
 * \brief
 * Creates the layout and connects the widgets
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "main.h"
#include "board.h"
#include "clients.h"

/*!
 * \brief
 * Border spacing around most widgets
 */
#define BORDER 3

static gboolean
animation_timeout_callback(gpointer user_data);

/*!
 * \brief
 * Enumeration of GtkListStore columns
 */
enum {
  PLAYER_COLUMN,     /*!< player information to display to the user */
  DESC_COLUMN,       /*!< description of the move to display to the user */
  BOARD_COLUMN,      /*!< string representing the board setup */
  MOVES_COLUMN,      /*!< list of moves/jumps leading to the current setup */
  IS_CLIENT0_COLUMN, /*!< \c TRUE if the client ID is 0, \c FALSE otherwise */
  STDOUT_COLUMN,     /*!< buffer to store stdout data for the current move */
  N_COLUMNS          /*!< number of columns (end of enum) */
};

/*!
 * \brief
 * String representation of the current board setup
 *
 * \attention
 * Must be freed before it's overwritten

 * \sa draw_board
 */
static gchar  *str_board  = NULL;
/*!
 * \brief
 * Singly-linked list of moves leading to the current board setup
 *
 * \attention
 * Should not be freed, as it's only a snapshot from the store
 */
static GSList *list_moves = NULL;

/*! \brief GtkDrawingArea where the graphical board representation is drawn */
static GtkWidget *drawing_area;
/*! \brief GtkButton that starts or kills the children */
static GtkWidget *btn_run_kill;
/*! \brief GtkToggleButton that controls animation */
static GtkWidget *btn_animate;
/*!
 * \brief
 * GtkStatusbar that provides information primarily about the children
 */
static GtkWidget *statusbar;
/*! \brief Context ID to use when updating the statusbar */
static guint statusbar_context_id;
/*!
 * \brief
 * Event source for the timeout event
 *
 * Set to zero when no timeout source is active, and non-zero otherwise.
 */
static guint source_timeout = 0;
/*!
 * \brief
 * Indicates whether a timeout event has been triggered without any data to be
 * displayed
 *
 * If set to \c TRUE, new data should be displayed as soon as it becomes
 * available, and a new timeout event should be added to continue the
 * animation.
 */
static gboolean is_animation_stalled = FALSE;
/*! \brief GtkWindow for the main program window */
static GtkWidget *window;
/*! \brief Array of GtkEntry for the client command lines */
static GtkWidget *entry_cmds[NUM_CLIENTS];
/*! \brief GtkTreeView with the list of board setups and moves */
static GtkWidget *list;
/*! \brief Array of GtkTextView for each of the clients and output types */
static GtkWidget *textviews[NUM_CHANNELS];
/*! \brief Buffers belonging to ::textviews */
static GtkTextBuffer *buffers[NUM_CHANNELS];
/*! \brief Indicates whether any of the clients is currently running */
static gboolean is_running = FALSE;

/*!
 * \brief
 * Gets the name of the starting textmark
 *
 * \param[in] row  the number of the row to generate the string for
 *
 * \return
 * the name of the starting textmark in a string that should be freed by the
 * caller
 */
static gchar *
get_mark_name_begin(gint row)
{
  return g_strdup_printf("begin_%04d", row);
}

/*!
 * \brief
 * Gets the name of the ending textmark
 *
 * \param[in] row  the number of the row to generate the string for
 *
 * \return
 * the name of the ending textmark in a string that should be freed by the
 * caller
 */
static gchar *
get_mark_name_end(gint row)
{
  return g_strdup_printf("end_%04d", row);
}

/*!
 * \brief
 * Starts or restarts animation
 */
static void
start_animation_timeout(void)
{
  extern guint option_timeout_ms;
  source_timeout = g_timeout_add(option_timeout_ms,
                                 (GSourceFunc)animation_timeout_callback,
                                 NULL);
  is_animation_stalled = FALSE;
}

/*!
 * \brief
 * Parses the line that the client wrote to standard output
 *
 * \param[in]  move_line    the string to be parsed
 * \param[out] board        the board setup described by the input
 * \param[out] moves        the set of moves or jumps described by the input
 * \param[out] description  a description of the move to display to the user
 * \param[out] player       a string describing which player made the move, or
 *                          \c NULL if it's a special move
 *
 * \return
 * whether the line was successfully parsed
 */
static gboolean
parse_client_stdout(gchar   *move_line,
                    gchar  **board,
                    GSList **moves,
                    gchar  **description,
                    gchar  **player)
{
  gchar **split_line;
  guint n_squares;
  int action;

  assert(board != NULL && *board == NULL);
  assert(moves != NULL && *moves == NULL);
  assert(description != NULL && *description == NULL);
  assert(player != NULL && *player == NULL);

  if (move_line == NULL) return FALSE;

  /* split_line needs to be freed before every return statement */
  split_line = g_strsplit(move_line, " ", 0);

  /* we need at least the three first sequences to parse */
  if (g_strv_length(split_line) < 3) {
    g_strfreev(split_line);
    return FALSE;
  }

  /* the first sequence needs to correspond to the board size */
  if (strlen(split_line[0]) != NUM_DARK_SQ) {
    g_strfreev(split_line);
    return FALSE;
  }

  {
    gchar **strv_moves;
    /* strv_moves needs to be freed before leaving this block */
    strv_moves = g_strsplit(split_line[1], "_", 0);
    n_squares = g_strv_length(strv_moves);

    /* str_action should be non-empty, and thus strv_moves as well */
    assert(n_squares != 0);

    action = atoi(strv_moves[0]);

    /* the action identifier is not one of the squares */
    --n_squares;

    /* verify that the action is legal and that it has a corresponding
       number of moves in the sequence */
    if ((action  < -5) ||
        (action  <  0  &&  n_squares != 0) ||
        (action ==  0  &&  n_squares != 2) ||
        (action  >  0  &&  n_squares != 1 + (guint)action)) {
      g_strfreev(strv_moves);
      g_strfreev(split_line);
      return FALSE;
    }

    /* early return if this is a special action */
    if (action < 0) {
      static const gchar * const special_actions[5] = {
        "Initial setup", /* -1 */
        "Red wins",      /* -2 */
        "White wins",    /* -3 */
        "Draw",          /* -4 */
        "Null move",     /* -5 */
      };

      *description = g_strdup(special_actions[-1-action]);
      *board = g_strdup(split_line[0]);
      g_strfreev(strv_moves);
      g_strfreev(split_line);
      return TRUE;
    }

    /* read the integers from the array of squares */
    /* read backwards, since prepending the linked list is cheaper */
    for (guint i = n_squares; i != 0; --i) {
      const int sq = atoi(strv_moves[i]);

      /* verify that the number is in range, otherwise quit */
      if (sq < 1 || sq > NUM_DARK_SQ) {
        /* illegal value, restore and abort */
        g_slist_free(*moves);
        *moves = NULL;
        g_strfreev(strv_moves);
        g_strfreev(split_line);
        return FALSE;
      }
      assert(sq > 0);
      *moves = g_slist_prepend(*moves, GUINT_TO_POINTER((guint)sq - 1));
    }
    g_strfreev(strv_moves);
  }

  *player = g_strdup(strcmp(split_line[2], "r") == 0 ? "[W]" : "[R]");
  *board = g_strdup(split_line[0]);

  /* generate the description string and the list of moves */
  {
    GSList *move = *moves;
    /* moves are written "A-B", and jumps "AxB", "AxBxC", ... */
    const gchar * const append_string = (action == 0) ? "-%u" : "x%u";

    /* a reasonable starting length is the length of the input string */
    GString *desc = g_string_sized_new(strlen(split_line[1]));
    guint old;

    /* there should be at least two squares in the list */
    assert(move != NULL);
    old = GPOINTER_TO_UINT(move->data);

    /* write the first value, without leading "-" or "x" */
    g_string_append_printf(desc, "%u", old + 1);

    while ((move = g_slist_next(move)) != NULL) {
      const guint new = GPOINTER_TO_UINT(move->data);
      g_string_append_printf(desc, append_string, new + 1);

      /* figure out which square we're jumping over, and mark it "x" */
      if (action > 0) (*board)[(old + new)/2 + ((new&4) == 0)] = 'x';

      old = new;
    }
    /* free the GString but keep the character data */
    *description = g_string_free(desc, FALSE);
  }

  g_strfreev(split_line);
  return TRUE;
}

/* documented in gui.h */
void
append_text(const gchar *text, gsize len, guint8 channel_id)
{
  gchar *player_column = NULL;
  gchar *desc_column   = NULL;
  gchar *board_column  = NULL;
  GSList *moves_column = NULL;
  gchar *stdout_column = NULL;
  GtkListStore *store;
  GtkTreeIter iter;
  gint nrows; /* number of rows except for the currently added/edited */

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
  nrows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);

  /* using 'switch' instead of 'if' to provide fallthrough into 'else' */
  switch ((int)(nrows > 0 &&
                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter,
                                              NULL, nrows - 1))) {
  case TRUE: {
    /* at least one entry exists - check if this should be updated */
    gboolean is_client0;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                       IS_CLIENT0_COLUMN, &is_client0,
                       STDOUT_COLUMN, &stdout_column,
                       -1);
    /* did we receive more data from the same client? */
    if ((CLIENT_ID(channel_id) == 0) == is_client0) {
      --nrows;
      break;
    }
    g_free(stdout_column);
    stdout_column = NULL;
    /* FALLTHROUGH */
  }
  default: {
    /* new row: create new buffer textmarks and a new entry in the store */
    gchar *mark_name_begin;
    gchar *mark_name_end;

    /* add buffer textmarks */
    mark_name_begin = get_mark_name_begin(nrows);
    mark_name_end = get_mark_name_end(nrows);
    for (guint8 i = 0; i < NUM_CHANNELS; ++i) {
      GtkTextIter it;
      gtk_text_buffer_get_end_iter(buffers[i], &it);
      gtk_text_buffer_create_mark(buffers[i], mark_name_begin, &it, TRUE);
      gtk_text_buffer_get_end_iter(buffers[i], &it);
      gtk_text_buffer_create_mark(buffers[i], mark_name_end, &it, TRUE);
    }
    g_free(mark_name_begin);
    g_free(mark_name_end);

    /* add store entry - nb: the 'row-inserted' callback will try to read
       the textmarks, and assumes that they have already been created */
    gtk_list_store_append(store, &iter);
  }
  }

  /* add text to the relevant buffer and move the ending textmark */
  {
    GtkTextIter iter;
    GtkTextMark *mark;
    gchar *mark_name_end;
    GtkTextBuffer *buffer = buffers[channel_id];
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, text, len);
    mark_name_end = get_mark_name_end(nrows);
    mark = gtk_text_buffer_get_mark(buffer, mark_name_end);
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_move_mark(buffer, mark, &iter);
    g_free(mark_name_end);
  }

  /* concatenate strings if stdout data was received more than once */
  if (IS_STDOUT(channel_id)) {
    gchar *buffer;
    /* get null-terminated string */
    buffer = g_strndup(text, len);
    if (stdout_column == NULL) {
      stdout_column = buffer;
    } else {
      gchar *temp = g_strconcat(stdout_column, buffer, NULL);
      g_free(stdout_column);
      g_free(buffer);
      stdout_column = temp;
    }
  }

  if (parse_client_stdout(stdout_column, &board_column, &moves_column,
                          &desc_column, &player_column)) {
    assert(board_column != NULL);
    assert(desc_column != NULL);
  } else {
    assert(board_column == NULL);
    assert(moves_column == NULL);
    assert(desc_column == NULL);
    assert(player_column == NULL);

    desc_column = g_strdup("Unparsable move");
  }

  /* update the store entry */
  gtk_list_store_set(store, &iter,
                     PLAYER_COLUMN, player_column,
                     DESC_COLUMN, desc_column,
                     BOARD_COLUMN, board_column,
                     MOVES_COLUMN, moves_column,
                     IS_CLIENT0_COLUMN, CLIENT_ID(channel_id) == 0,
                     STDOUT_COLUMN, stdout_column,
                     -1);
  g_free(player_column);
  g_free(desc_column);
  g_free(board_column);
  g_free(stdout_column);
}

/*!
 * \brief
 * Releases the linked list of moves, stored in a datastore entry
 *
 * Should be called for each entry in the store. The function follows the
 * signature of \c GtkTreeModelForeachFunc.
 *
 * \param[in] model      the \c GtkTreeModel that is being iterated
 * \param[in] path       not used
 * \param[in] iter       the current \c GtkTreeIter
 * \param[in] user_data  not used
 *
 * \return
 * \c FALSE (to continue iterating)
 */
static gboolean
free_move_callback(GtkTreeModel *model,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   gpointer      user_data)
{
  GSList *moves_column;

  UNUSED(path);
  UNUSED(user_data);

  gtk_tree_model_get(model, iter,
                     MOVES_COLUMN, &moves_column,
                     -1);
  g_slist_free(moves_column);
  return FALSE;
}

/*!
 * \brief
 * Releases and clears information in the store
 */
static void
release_resources(void)
{
  GtkListStore *store;

  g_free(str_board);
  str_board = NULL;
  list_moves = NULL;

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
  gtk_tree_model_foreach(GTK_TREE_MODEL(store),
                         (GtkTreeModelForeachFunc)free_move_callback,
                         NULL);
  gtk_list_store_clear(store);
}

/*!
 * \brief
 * Creates new buffers for the textviews
 */
static void
wipe_buffers(void)
{
  for (guint8 i = 0; i < NUM_CHANNELS; ++i) {
    buffers[i] = gtk_text_buffer_new(NULL);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(textviews[i]), buffers[i]);
    gtk_text_buffer_create_tag(buffers[i], "emph",
                               "background", "#FFFF00",
                               NULL);
    g_object_unref(buffers[i]);
  }
}

/*!
 * \brief
 * Gets a string describing the state of the client
 *
 * \param[in] n      a zero-based numbering of the client
 * \param[in] client the \ref client_t object to examine for state information
 *
 * \return
 * a string description of the current state, that should be freed by the
 * caller when it's not needed anymore
 */
static gchar *
get_client_description(guint16 n, const client_t * const client)
{
  ++n;
  if (client->is_running) {
    return g_strdup_printf("Player %" G_GUINT16_FORMAT
                           " (pid %d) is running.", n, client->pid);
  } else {
    return g_strdup_printf("Player %" G_GUINT16_FORMAT
                           " (pid %d) exited with status %d.",
                           n, client->pid, client->status);
  }
}

/* documented in gui.h */
void
update_status(const client_t clients[static NUM_CLIENTS])
{
  gchar *text;
  is_running = FALSE;
  {
    gchar *descriptions[NUM_CLIENTS+1];
    for (guint8 i=0; i<NUM_CLIENTS; ++i) {
      descriptions[i] = get_client_description(i, &clients[i]);
      is_running |= clients[i].is_running;
    }
    descriptions[NUM_CLIENTS] = NULL;

    assert(g_strv_length(descriptions) == NUM_CLIENTS);

    text = g_strjoinv(" ", descriptions);
    for (guint8 i=0; i<NUM_CLIENTS; ++i) g_free(descriptions[i]);
  }

  gtk_statusbar_pop(GTK_STATUSBAR(statusbar), statusbar_context_id);
  gtk_statusbar_push(GTK_STATUSBAR(statusbar), statusbar_context_id, text);
  g_free(text);

  gtk_button_set_label(GTK_BUTTON(btn_run_kill), is_running ? "Kill" : "Run");
  if (!is_running) {
    /* hack to update the board if redrawing was delayed */
    gtk_widget_queue_draw(drawing_area);
  }
}

/*!
 * \brief
 * Callback for when the drawing area needs to be redrawn
 *
 * \param[in] widget     the widget that received the signal
 * \param[in] event      not used
 * \param[in] user_data  not used
 *
 * \return
 * \c TRUE (to stop other handlers from being invoked for the event)
 */
static gboolean
expose_event_callback(GtkWidget      *widget,
                      GdkEventExpose *event,
                      gpointer        user_data)
{
  cairo_t *cr;

  UNUSED(event);
  UNUSED(user_data);

  cr = gdk_cairo_create(gtk_widget_get_window(widget));
  draw_board(cr, widget->allocation.width, widget->allocation.height,
             str_board, list_moves);
  cairo_destroy(cr);
  return TRUE;
}

/*!
 * \brief
 * Callback for when the drawing area is about to get resized
 *
 * \param[in] widget      the widget that received the signal
 * \param[in] allocation  the widget's allocated region
 * \param[in] user_data   not used
 */
static void
size_allocate_callback(GtkWidget     *widget,
                       GtkAllocation *allocation,
                       gpointer       user_data)
{
  UNUSED(user_data);

  /* force the width to be equal to the height */
  gtk_widget_set_size_request(widget, allocation->height, -1);
}

/* documented in gui.h */
void
print_error(gchar *message)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  "%s", message);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

/*!
 * \brief
 * Callback for when the animation timed out and it's time to change the
 * selected row
 *
 * \param[in] user_data  not used
 *
 * \return
 * \c FALSE (to remove the source)
 */
static gboolean
animation_timeout_callback(gpointer user_data)
{
  GtkTreeSelection *selection;
  GList *rows;
  GtkListStore *store;
  GtkTreeIter iter;

  UNUSED(user_data);

  source_timeout = 0;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
  rows = gtk_tree_selection_get_selected_rows(selection, NULL);

  if (g_list_first(rows) != NULL) {
    GtkTreePath *path;
    path = (GtkTreePath *)g_list_first(rows)->data;
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);

    if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
      GtkTreePath *path;
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
      gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL, FALSE);
      gtk_tree_path_free(path);
    } else {
      extern gboolean option_quit;
      if (!is_running && option_quit) {
        gtk_widget_destroy(window);
      }
      is_animation_stalled = TRUE;
    }
  }

  g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(rows);

  return FALSE;
}

/*!
 * \brief
 * \c 'clicked' callback for the button that says either 'Run' or 'Kill'
 *
 * \param[in] widget     not used
 * \param[in] user_data  not used
 */
static void
run_kill_clicked_callback(GtkWidget *widget, gpointer user_data)
{
  GError *error = NULL;

  UNUSED(widget);
  UNUSED(user_data);

  if (is_running) {
    kill_clients();
  } else {
    const gchar *cmds[2];

    /* clear data that might exist from a previous run */
    release_resources();
    wipe_buffers();

    cmds[0] = gtk_entry_get_text(GTK_ENTRY(entry_cmds[0]));
    cmds[1] = gtk_entry_get_text(GTK_ENTRY(entry_cmds[1]));
    launch_clients(cmds, &error);
    if (error != NULL) {
      print_error(error->message);
      g_error_free(error);
    }
  }
}

/*!
 * \brief
 * Callback for when the 'Animate' button is clicked
 *
 * \param[in] button     the button that received the signal
 * \param[in] user_data  not used
 */
static void
animate_toggled_callback(GtkToggleButton *button, gpointer user_data)
{
  UNUSED(user_data);

  if (gtk_toggle_button_get_active(button)) {
    start_animation_timeout();
  } else {
    if (source_timeout > 0) {
      g_source_remove(source_timeout);
      source_timeout = 0;
    }
  }
}

/*!
 * \brief
 * Gets board appearance from the store, to display in the drawing area
 *
 * \param[in] model  the store from which to read
 * \param[in] iter   an iterator to the current store entry
 *
 * \pre
 * ::str_board and ::list_moves must be freed and set to \c NULL prior to
 * calling this function, to avoid memory leaks
 */
static void
load_board_and_moves(GtkTreeModel *model, GtkTreeIter iter)
{
  assert(str_board == NULL);
  assert(list_moves == NULL);

  gtk_tree_model_get(model, &iter,
                     BOARD_COLUMN, &str_board,
                     MOVES_COLUMN, &list_moves,
                     -1);
  /* hack to avoid flickering - delay redrawing if we expect
     to get something to draw soon */
  if (!is_running || str_board != NULL ||
      gtk_tree_model_iter_next(model, &iter)) {
    gtk_widget_queue_draw(drawing_area);
  }
}

/*!
 * \brief
 * Highlights text in the relevant text buffers
 *
 * \param[in] path  the path to the list row that has been selected
 */
static void
highlight_text(GtkTreePath *path)
{
  gint row;
  gchar *mark_name_begin;
  gchar *mark_name_end;

  row = *gtk_tree_path_get_indices(path);
  mark_name_begin = get_mark_name_begin(row);
  mark_name_end = get_mark_name_end(row);

  for (guint8 i = 0; i < NUM_CHANNELS; ++i) {
    GtkTextIter iter_begin;
    GtkTextIter iter_end;
    GtkTextMark *mark_begin;
    GtkTextMark *mark_end;

    /* clear old highlighting */
    gtk_text_buffer_get_start_iter(buffers[i], &iter_begin);
    gtk_text_buffer_get_end_iter(buffers[i], &iter_end);
    gtk_text_buffer_remove_tag_by_name(buffers[i], "emph",
                                       &iter_begin, &iter_end);

    /* create new highlighting */
    mark_begin = gtk_text_buffer_get_mark(buffers[i], mark_name_begin);
    mark_end   = gtk_text_buffer_get_mark(buffers[i], mark_name_end);

    gtk_text_buffer_get_iter_at_mark(buffers[i], &iter_begin, mark_begin);
    gtk_text_buffer_get_iter_at_mark(buffers[i], &iter_end,   mark_end);
    gtk_text_buffer_apply_tag_by_name(buffers[i], "emph",
                                      &iter_begin, &iter_end);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textviews[i]), mark_begin,
                                 .0, TRUE, .0, .0);
  }

  g_free(mark_name_begin);
  g_free(mark_name_end);
}

/*!
 * \brief
 * Callback for when a row is selected in the tree view
 *
 * \param[in] widget     the widget that received the signal
 * \param[in] user_data  not used
 */
static void
cursor_changed_callback(GtkWidget *widget, gpointer user_data)
{
  GtkTreeSelection *selection;
  GList *rows;

  UNUSED(user_data);

  g_free(str_board);
  str_board = NULL;
  list_moves = NULL;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
  rows = gtk_tree_selection_get_selected_rows(selection, NULL);

  /* we want one and only one row to have been selected */
  if (g_list_first(rows) != NULL && g_list_next(rows) == NULL) {
    GtkTreePath *path;
    GtkListStore *store;
    GtkTreeIter iter;

    path = (GtkTreePath *)g_list_first(rows)->data;
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
    load_board_and_moves(GTK_TREE_MODEL(store), iter);

    highlight_text(path);
  }

  g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(rows);

  /* abort current animation and restart it */
  if (source_timeout > 0) {
    g_source_remove(source_timeout);
    source_timeout = 0;
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_animate))) {
    start_animation_timeout();
  }
}

/*!
 * \brief
 * Callback for when data was changed in the store, as it might affect what's
 * currently displayed
 *
 * \param[in]  model      the model on which the signal was emitted
 * \param[in]  path       a path identifying the changed row
 * \param[in]  iter       an iterator pointing to the changed row
 * \param[out] user_data  not used
 */
static void
row_changed_callback(GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      user_data)
{
  GtkTreeSelection *selection;

  UNUSED(user_data);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
  if (gtk_tree_selection_path_is_selected(selection, path)) {
    g_free(str_board);
    str_board = NULL;
    list_moves = NULL;
    load_board_and_moves(model, *iter);

    highlight_text(path);
  }
}

/*!
 * \brief
 * Callback for when data was inserted into the store, as it might cause us to
 * select another row
 *
 * \param[in]  model      not used
 * \param[in]  path       a path identifying the new row
 * \param[in]  iter       not used
 * \param[out] user_data  not used
 */
static void
row_inserted_callback(GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      user_data)
{
  GtkTreeSelection *selection;

  UNUSED(model);
  UNUSED(iter);
  UNUSED(user_data);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

  /* abort if an animation is in progress */
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_animate)) &&
      !is_animation_stalled &&
      gtk_tree_selection_count_selected_rows(selection) == 1) return;

  gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL, FALSE);
}

/*!
 * \brief
 * Callback for when the main window is about to be destroyed
 *
 * Cleans up before the program terminates, to avoid orphan processes.
 *
 * \param[in] object     not used
 * \param[in] user_data  not used
 */
static void
window_destroy_callback(GtkObject *object,
                        gpointer   user_data)
{
  UNUSED(object);
  UNUSED(user_data);

  /* we might not receive a signal when the clients exit, but this should
     at least get the ball rolling */
  kill_clients();

  release_resources();

  gtk_main_quit();
}

/*!
 * \brief
 * Builds a single stream output text view and its label
 *
 * \param[in]  name      the label to place above the text view
 * \param[out] textview  the generated GtkTextView
 * \param[out] buffer    the buffer belonging to the text view widget
 *
 * \return
 * a widget wrapping everything, to be added to the layout by the caller
 */
static GtkWidget *
create_player_buffer(const gchar    *name,
                     GtkWidget     **textview,
                     GtkTextBuffer **buffer)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *scrolledwindow;

  box = gtk_vbox_new(FALSE, 0);
  label = gtk_label_new(name);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
  *textview = gtk_text_view_new();
  gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(box), scrolledwindow);
  gtk_container_add(GTK_CONTAINER(scrolledwindow), *textview);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(*textview), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(*textview), GTK_WRAP_WORD);
  {
    extern gchar *option_font;
    PangoFontDescription *fontdesc;
    fontdesc = pango_font_description_from_string(option_font);
    gtk_widget_modify_font(*textview, fontdesc);
    pango_font_description_free(fontdesc);
  }
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(*textview));
  return box;
}

/*!
 * \brief
 * Creates a panel for one player, with stdout, stderr and command line
 *
 * \param[in] id  a zero-based client ID
 *
 * \return
 * a widget wrapping everything, to be added to the layout by the caller
 */
static GtkWidget *
create_player_panel(guint8 id)
{
  extern gchar *option_cmds[NUM_CLIENTS];
  GtkWidget *frame_outer;
  GtkWidget *box;
  GtkWidget *paned;
  GtkWidget *box_inner;
  GtkWidget *label_cmd;

  assert(id < NUM_CLIENTS);

  {
    gchar *name;
    name = g_strdup_printf("Player %u", (unsigned int)id + 1);
    frame_outer = gtk_frame_new(name);
    g_free(name);
  }
  box = gtk_vbox_new(FALSE, BORDER);
  gtk_container_set_border_width(GTK_CONTAINER(box), BORDER);
  paned = gtk_vpaned_new();
  gtk_container_add(GTK_CONTAINER(frame_outer), box);
  gtk_box_pack_start(GTK_BOX(box), paned, TRUE, TRUE, 0);
  gtk_paned_pack1(GTK_PANED(paned),
                  create_player_buffer("Standard Output:",
                                       &textviews[CHANNEL_ID(id, STDOUT)],
                                       &buffers[CHANNEL_ID(id, STDOUT)]),
                  TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(paned),
                  create_player_buffer("Standard Error:",
                                       &textviews[CHANNEL_ID(id, STDERR)],
                                       &buffers[CHANNEL_ID(id, STDERR)]),
                  TRUE, TRUE);
  label_cmd = gtk_label_new("Command line:");
  entry_cmds[id] = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_cmds[id]),
                     option_cmds[id] ? option_cmds[id] : "");
  box_inner = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_inner), label_cmd, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(label_cmd), 0, 0);
  gtk_container_add(GTK_CONTAINER(box_inner), entry_cmds[id]);
  gtk_box_pack_start(GTK_BOX(box), box_inner, FALSE, FALSE, 0);
  return frame_outer;
}

/* documented in gui.h */
void
create_window_with_widgets(void)
{
  extern gboolean option_run;
  extern gboolean option_maximize;
  extern gint     option_width_px;
  extern gint     option_height_px;

  GtkWidget *paned;

  /* create the main window */
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Checkers Visualizer");
  gtk_window_set_default_size(GTK_WINDOW(window),
                              option_width_px, option_height_px);
  if (option_maximize) {
    gtk_window_maximize(GTK_WINDOW(window));
  }
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(window_destroy_callback), NULL);

  /* initialize the data model for the GtkTreeView */
  {
    GtkCellRenderer *renderer1;
    GtkCellRenderer *renderer2;
    GtkTreeViewColumn *column1;
    GtkTreeViewColumn *column2;
    GtkListStore *store;
    GtkTreeSelection *selection;

    list = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
    renderer1 = gtk_cell_renderer_text_new();
    column1 = gtk_tree_view_column_new_with_attributes("Player", renderer1,
                                                       "text", PLAYER_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column1);
    renderer2 = gtk_cell_renderer_text_new();
    column2 = gtk_tree_view_column_new_with_attributes("Move", renderer2,
                                                       "text", DESC_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column2);
    store = gtk_list_store_new(N_COLUMNS,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_POINTER,
                               G_TYPE_BOOLEAN,
                               G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect(store, "row-changed",
                     G_CALLBACK(row_changed_callback), NULL);
    g_signal_connect(store, "row-inserted",
                     G_CALLBACK(row_inserted_callback), NULL);
    g_object_unref(store);

    g_signal_connect(list, "cursor-changed",
                     G_CALLBACK(cursor_changed_callback), NULL);
  }

  /* build outmost wrapper to contain the GtkPaned and GtkStatusbar */
  {
    GtkWidget *box;

    box = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);

    paned = gtk_vpaned_new();
    gtk_container_set_border_width(GTK_CONTAINER(paned), BORDER);
    gtk_box_pack_start(GTK_BOX(box), paned, TRUE, TRUE, 1);

    statusbar = gtk_statusbar_new();
    statusbar_context_id =
      gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "Statusbar");
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), statusbar_context_id,
                       "Ready");
    gtk_box_pack_start(GTK_BOX(box), statusbar, FALSE, FALSE, 1);
  }

  /* build drawing area, list of moves and buttons */
  {
    GtkWidget *table;

    table = gtk_table_new(2, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), BORDER);
    gtk_table_set_row_spacings(GTK_TABLE(table), BORDER);
    gtk_paned_pack1(GTK_PANED(paned), table, TRUE, TRUE);

    /* create the drawing area */
    {
      GtkWidget *frame;
      GtkWidget *box_padding;

      /* GtkAspectFrame doesn't cut it - rolling my own */
      frame = gtk_frame_new("Board");
      gtk_table_attach(GTK_TABLE(table), frame, 0, 1, 0, 2,
                       GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      /* GtkDrawingArea that should be kept square */
      drawing_area = gtk_drawing_area_new();
      box_padding = gtk_vbox_new(FALSE, BORDER);
      gtk_container_set_border_width(GTK_CONTAINER(box_padding), BORDER);
      gtk_container_add(GTK_CONTAINER(frame), box_padding);
      gtk_container_add(GTK_CONTAINER(box_padding), drawing_area);
      g_signal_connect(G_OBJECT(drawing_area), "expose_event",
                       G_CALLBACK(expose_event_callback), NULL);
      g_signal_connect(G_OBJECT(drawing_area), "size-allocate",
                       G_CALLBACK(size_allocate_callback), NULL);
    }
    /* create the list of moves */
    {
      GtkWidget *frame;
      GtkWidget *box_padding;
      GtkWidget *scrolledwindow;

      frame = gtk_frame_new("Moves");
      box_padding = gtk_vbox_new(FALSE, BORDER);
      gtk_container_set_border_width(GTK_CONTAINER(box_padding), BORDER);
      gtk_container_add(GTK_CONTAINER(frame), box_padding);
      scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
      gtk_container_add(GTK_CONTAINER(scrolledwindow), list);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                                     GTK_POLICY_AUTOMATIC,
                                     GTK_POLICY_AUTOMATIC);
      gtk_container_add(GTK_CONTAINER(box_padding), scrolledwindow);
      gtk_table_attach(GTK_TABLE(table), frame, 1, 2, 0, 1,
                       GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    }
    /* place the buttons */
    {
      extern gboolean option_animate;
      GtkWidget *box;

      box = gtk_hbox_new(FALSE, 0);
      btn_run_kill = gtk_button_new_with_label("Run");
      gtk_widget_set_size_request(btn_run_kill, 80, 35);
      gtk_box_pack_end(GTK_BOX(box), btn_run_kill, FALSE, FALSE, 0);
      btn_animate = gtk_toggle_button_new_with_label("Animate");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_animate),
                                   option_animate);
      gtk_widget_set_size_request(btn_animate, 80, 35);
      gtk_box_pack_end(GTK_BOX(box), btn_animate, FALSE, FALSE, 0);
      gtk_table_attach(GTK_TABLE(table), box, 1, 2, 1, 2,
                       GTK_EXPAND | GTK_FILL, 0, 0, 0);
      g_signal_connect(btn_run_kill, "clicked",
                       G_CALLBACK(run_kill_clicked_callback), NULL);
      g_signal_connect(btn_animate, "toggled",
                       G_CALLBACK(animate_toggled_callback), NULL);
    }
  }

  /* build player output and command line section */
  {
    GtkWidget *paned_players;

    paned_players = gtk_hpaned_new();
    gtk_paned_pack2(GTK_PANED(paned), paned_players, FALSE, TRUE);
    for (guint8 i = 0; i < NUM_CLIENTS; ++i) {
      (i == 0 ? gtk_paned_pack1 : gtk_paned_pack2)
        (GTK_PANED(paned_players), create_player_panel(i), TRUE, TRUE);
    }
  }

  gtk_widget_show_all(window);

  if (option_run) {
    gtk_button_clicked(GTK_BUTTON(btn_run_kill));
  }
}
