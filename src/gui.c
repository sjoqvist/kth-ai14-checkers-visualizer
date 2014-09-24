#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "main.h"
#include "board.h"
#include "clients.h"

#define BORDER 3

static gboolean
animation_timeout_callback(gpointer user_data);

extern gchar   *default_cmd1;
extern gchar   *default_cmd2;
extern gboolean default_animate;
extern gboolean default_run;
extern guint    default_timeout;

extern gchar   *default_font;
extern gboolean default_maximize;
extern gboolean default_quit;
extern gint     default_width;
extern gint     default_height;

enum {
  PLAYER_COLUMN,
  DESC_COLUMN,
  BOARD_COLUMN,
  MOVES_COLUMN,
  IS_CLIENT1_COLUMN,
  STDOUT_COLUMN,
  N_COLUMNS
};

static gchar  *str_board  = NULL;
static GSList *list_moves = NULL;

static GtkWidget *drawing_area;
static GtkWidget *btn_run_kill;
static GtkWidget *btn_animate;
static GtkWidget *statusbar;
static guint statusbar_context_id;
static guint source_timeout = 0;
static gboolean is_animation_stalled = FALSE;
static GtkWidget *window;
static GtkWidget *entry_player1;
static GtkWidget *entry_player2;
static GtkWidget *list;
static GtkWidget *textviews[4];
static GtkTextBuffer *buffers[4];
static gboolean is_running = FALSE;

/* get the name of the starting textmark - result must be freed */
static gchar *
get_mark_name_begin(gint row)
{
  return g_strdup_printf("begin_%04d", row);
}

/* get the name of the ending textmark - result must be freed */
static gchar *
get_mark_name_end(gint row)
{
  return g_strdup_printf("end_%04d", row);
}

/* start or restart animation */
static void
start_animation_timeout()
{
  source_timeout = g_timeout_add(default_timeout,
                                 animation_timeout_callback, NULL);
  is_animation_stalled = FALSE;
}

/* add incoming text to a buffer, and save it in the store */
void
append_text(const gchar *text, gsize len,
            gboolean is_client1, gboolean is_stdout)
{
  gchar *player_column = NULL;
  gchar *desc_column;
  gchar *board_column = NULL;
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
    gboolean is_client1_temp;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                       IS_CLIENT1_COLUMN, &is_client1_temp,
                       STDOUT_COLUMN, &stdout_column,
                       -1);
    /* did we receive more data from the same client? */
    if (is_client1 == is_client1_temp) {
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
    guint8 i;

    /* add buffer textmarks */
    mark_name_begin = get_mark_name_begin(nrows);
    mark_name_end = get_mark_name_end(nrows);
    for (i = 0; i < 4; ++i) {
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
    GtkTextBuffer *buffer = buffers[is_client1*2+is_stdout];
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, text, len);
    mark_name_end = get_mark_name_end(nrows);
    mark = gtk_text_buffer_get_mark(buffer, mark_name_end);
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_move_mark(buffer, mark, &iter);
    g_free(mark_name_end);
  }

  /* default description (hopefully gets overwritten) */
  desc_column = g_strdup("Unparsable move");

  /* concatenate strings if stdout data was received more than once */
  if (is_stdout) {
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
  /* parse the move */
  {
    gchar board[34];
    gchar moves_str[256];
    int moves[256];
    gchar player;
    if (stdout_column != NULL &&
        sscanf(stdout_column, "%33s %255s %c", board,
               moves_str, &player) == 3 &&
        strlen(board) == 32) {
      guint moves_length;
      gchar *player_temp = NULL;
      gchar *desc_temp = NULL;
      {
        gchar **movesv;
        guint i;
        movesv = g_strsplit(moves_str, "_", 0);
        moves_length = g_strv_length(movesv);
        if (moves_length > 0) {
          moves[0] = atoi(movesv[0]);
          if (moves[0] >= 0) {
            desc_temp = g_strjoinv(moves[0] == 0 ? "-" : "x", movesv + 1);
            player_temp = g_strdup(player == 'r' ? "[W]" : "[R]");
          }
          for (i = 1; i < moves_length; ++i) {
            moves[i] = atoi(movesv[i]) - 1;
            if (moves[i] < 0 || moves[i] > 31) {
              /* illegal value, avoid parsing */
              moves_length = 0;
            }
          }
        }
        g_strfreev(movesv);
      }
      if (moves_length == 1 && moves[0] >= -5 && moves[0] <= -1) {
        static const gchar * const special_actions[] = {
          "Null move",    /* -5 */
          "Draw",         /* -4 */
          "White wins",   /* -3 */
          "Red wins",     /* -2 */
          "Initial setup" /* -1 */
        };
        g_free(desc_column);
        desc_column = g_strdup(special_actions[moves[0]+5]);
        board_column = g_strdup(board);
      } else if (moves_length == 3 && moves[0] == 0) {
        board_column = g_strdup(board);
        player_column = player_temp;
        g_free(desc_column);
        desc_column = desc_temp;
        while (--moves_length > 0) {
          moves_column = g_slist_append(moves_column,
                                        GINT_TO_POINTER(moves[moves_length]));
        }
      } else if (moves_length > 2 && moves[0] > 0 &&
                 moves_length == 2 + (guint)moves[0]) {
        guint i;
        for (i = 2; i < moves_length; ++i) {
          board[(moves[i-1] + moves[i])/2 + ((moves[i]&4) == 0)] = 'x';
        }
        board_column = g_strdup(board);
        player_column = player_temp;
        g_free(desc_column);
        desc_column = desc_temp;
        while (--moves_length > 0) {
          moves_column = g_slist_append(moves_column,
                                        GINT_TO_POINTER(moves[moves_length]));
        }
      } else {
        g_free(player_temp);
        g_free(desc_temp);
      }
    }
  }

  /* update the store entry */
  gtk_list_store_set(store, &iter,
                     PLAYER_COLUMN, player_column,
                     DESC_COLUMN, desc_column,
                     BOARD_COLUMN, board_column,
                     MOVES_COLUMN, moves_column,
                     IS_CLIENT1_COLUMN, is_client1,
                     STDOUT_COLUMN, stdout_column,
                     -1);
  g_free(player_column);
  g_free(desc_column);
  g_free(board_column);
  g_free(stdout_column);
}

/* called for each linked list of moves to release the resource */
static gboolean
free_move(GtkTreeModel *model,
          GtkTreePath *path,
          GtkTreeIter *iter,
          gpointer user_data)
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

/* release and clear information in the store */
static void
free_store()
{
  GtkListStore *store;

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
  gtk_tree_model_foreach(GTK_TREE_MODEL(store), free_move, NULL);
  gtk_list_store_clear(store);
}

/* create new buffers for the textviews */
static void
wipe_buffers()
{
  guint8 i;
  for (i = 0; i < 4; ++i) {
    buffers[i] = gtk_text_buffer_new(NULL);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(textviews[i]), buffers[i]);
    gtk_text_buffer_create_tag(buffers[i], "emph",
                               "background", "#FFFF00",
                               NULL);
    g_object_unref(buffers[i]);
  }
}

/* get a string describing the state of the client */
static gchar *
get_client_description(guint16 n, const client_t * const client)
{
  if (client->is_running) {
    return g_strdup_printf("Player %" G_GUINT16_FORMAT
                           " (pid %d) is running.", n, client->pid);
  } else {
    return g_strdup_printf("Player %" G_GUINT16_FORMAT
                           " (pid %d) exited with status %d.",
                           n, client->pid, client->status);
  }
}

/* update the statusbar after a child process has spawned or exited */
void
update_status(const client_t clients[NUM_CLIENTS])
{
  gchar *text;
  is_running = FALSE;
  {
    gchar *descriptions[NUM_CLIENTS+1];
    guint8 i;
    for (i=0; i<NUM_CLIENTS; ++i) {
      descriptions[i] = get_client_description(i+1, &clients[i]);
      is_running |= clients[i].is_running;
    }
    descriptions[NUM_CLIENTS] = NULL;

    assert(g_strv_length(descriptions) == NUM_CLIENTS);

    text = g_strjoinv(" ", descriptions);
    for (i=0; i<NUM_CLIENTS; ++i) g_free(descriptions[i]);
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

/* callback for when the drawing area needs to be redrawn */
static gboolean
expose_event_callback(GtkWidget *widget,
                      GdkEventExpose *event,
                      gpointer user_data)
{
  cairo_t *cr;

  UNUSED(event);
  UNUSED(user_data);

  cr = gdk_cairo_create(widget->window);
  draw_board(cr, widget->allocation.width, widget->allocation.height,
             str_board, list_moves);
  cairo_destroy(cr);
  return TRUE;
}

/* callback for when the drawing area is about to get resized */
static void
size_allocate_callback(GtkWidget *widget,
                       GtkAllocation *allocation,
                       gpointer user_data)
{
  UNUSED(user_data);

  /* force the width to be equal to the height */
  gtk_widget_set_size_request(widget, allocation->height, -1);
}

/* display an error dialog box */
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
      if (!is_running && default_quit) {
        gtk_widget_destroy(window);
      }
      is_animation_stalled = TRUE;
    }
  }

  g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(rows);

  return FALSE;
}

/* 'clicked' callback for the button that says either 'Run' or 'Kill' */
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
    str_board = NULL;
    list_moves = NULL;
    free_store();
    wipe_buffers();

    cmds[0] = gtk_entry_get_text(GTK_ENTRY(entry_player1));
    cmds[1] = gtk_entry_get_text(GTK_ENTRY(entry_player2));
    launch_clients(cmds, &error);
    if (error != NULL) {
      print_error(error->message);
      g_error_free(error);
    }
  }
}

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

/* get board appearance from the store, to display in the drawing area */
static void
load_board_and_moves(GtkTreeModel *model, GtkTreeIter iter)
{
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

/* highlight text in the relevant text buffers */
static void
highlight_text(GtkTreePath *path)
{
  gint row;
  gchar *mark_name_begin;
  gchar *mark_name_end;
  guint8 i;

  row = *gtk_tree_path_get_indices(path);
  mark_name_begin = get_mark_name_begin(row);
  mark_name_end = get_mark_name_end(row);

  for (i = 0; i < 4; ++i) {
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

/* callback for when a row is selected in the tree view */
static void
cursor_changed(GtkWidget *widget, gpointer user_data)
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

/* data was changed in the store (might affect what's currently displayed) */
static void
row_changed_callback(GtkTreeModel *model,
                     GtkTreePath *path,
                     GtkTreeIter *iter,
                     gpointer user_data)
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

/* data was inserted into the store (might cause us to select another row ) */
static void
row_inserted_callback(GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer user_data)
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

/* clean up before the program terminates, to avoid orphan processes */
static void
window_destroy_callback(GtkObject *object,
                        gpointer   user_data)
{
  UNUSED(object);
  UNUSED(user_data);

  /* we might not receive a signal when the clients exit, but this should
     at least get the ball rolling */
  kill_clients();

  gtk_main_quit();
}

/* build a single stream output text view and its label */
static GtkWidget *
create_player_buffer(const gchar *name,
                     GtkWidget **textview,
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
    PangoFontDescription *fontdesc;
    fontdesc = pango_font_description_from_string(default_font);
    gtk_widget_modify_font(*textview, fontdesc);
    pango_font_description_free(fontdesc);
  }
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(*textview));
  return box;
}

/* create a panel for one player, with stdout, stderr and command line */
static GtkWidget *
create_player_panel(const gchar *name,
                    GtkWidget **textview_stdout,
                    GtkWidget **textview_stderr,
                    GtkTextBuffer **buffer_stdout,
                    GtkTextBuffer **buffer_stderr,
                    GtkWidget **entry,
                    const gchar *cmd)
{
  GtkWidget *frame_outer;
  GtkWidget *box;
  GtkWidget *paned;
  GtkWidget *box_inner;
  GtkWidget *label_cmd;

  frame_outer = gtk_frame_new(name);
  box = gtk_vbox_new(FALSE, BORDER);
  gtk_container_set_border_width(GTK_CONTAINER(box), BORDER);
  paned = gtk_vpaned_new();
  gtk_container_add(GTK_CONTAINER(frame_outer), box);
  gtk_box_pack_start(GTK_BOX(box), paned, TRUE, TRUE, 0);
  gtk_paned_pack1(GTK_PANED(paned),
                  create_player_buffer("Standard Output:", textview_stdout,
                                       buffer_stdout),
                  TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(paned),
                  create_player_buffer("Standard Error:", textview_stderr,
                                       buffer_stderr),
                  TRUE, TRUE);
  label_cmd = gtk_label_new("Command line:");
  *entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(*entry), cmd);
  box_inner = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_inner), label_cmd, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(label_cmd), 0, 0);
  gtk_container_add(GTK_CONTAINER(box_inner), *entry);
  gtk_box_pack_start(GTK_BOX(box), box_inner, FALSE, FALSE, 0);
  return frame_outer;
}

/* master function to build the GUI */
void
create_window_with_widgets()
{
  GtkWidget *paned;

  /* create the main window */
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Checkers Visualizer");
  gtk_window_set_default_size(GTK_WINDOW(window),
                              default_width, default_height);
  if (default_maximize) {
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
                     G_CALLBACK(cursor_changed), NULL);
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
      GtkWidget *box;

      box = gtk_hbox_new(FALSE, 0);
      btn_run_kill = gtk_button_new_with_label("Run");
      gtk_widget_set_size_request(btn_run_kill, 80, 35);
      gtk_box_pack_end(GTK_BOX(box), btn_run_kill, FALSE, FALSE, 0);
      btn_animate = gtk_toggle_button_new_with_label("Animate");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_animate),
                                   default_animate);
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
    gtk_paned_pack1(GTK_PANED(paned_players),
                    create_player_panel("Player 1",
                                        textviews + STDOUT1,
                                        textviews + STDERR1,
                                        buffers + STDOUT1,
                                        buffers + STDERR1,
                                        &entry_player1,
                                        default_cmd1),
                    TRUE, TRUE);
    gtk_paned_pack2(GTK_PANED(paned_players),
                    create_player_panel("Player 2",
                                        textviews + STDOUT2,
                                        textviews + STDERR2,
                                        buffers + STDOUT2,
                                        buffers + STDERR2,
                                        &entry_player2,
                                        default_cmd2),
                    TRUE, TRUE);
  }

  gtk_widget_show_all(window);

  if (default_run) {
    gtk_button_clicked(GTK_BUTTON(btn_run_kill));
  }
}
