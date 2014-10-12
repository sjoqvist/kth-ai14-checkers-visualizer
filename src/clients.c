#define _POSIX_C_SOURCE 1
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <gtk/gtk.h>
#include "clients.h"
#include "main.h"
#include "gui.h"

#define BUFFER_SIZE (64<<10)

static GIOChannel *channel_stdin[NUM_CLIENTS] = { NULL, NULL };

static client_t clients[NUM_CLIENTS] = {
  { 0, FALSE, 0 },
  { 0, FALSE, 0 }
};

static void
stop_channels(GIOChannel *channel_in, GIOChannel *channel_out)
{
  assert(channel_in != NULL);

  g_io_channel_shutdown(channel_in, FALSE, NULL);

  if (channel_out == NULL) return;

  g_io_channel_shutdown(channel_out, FALSE, NULL);
  g_io_channel_unref(channel_out);
}

/* callback for when new data as available in a pipe */
static gboolean
io_watch_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
  const gint input_type = GPOINTER_TO_INT(data);
  gchar buffer[BUFFER_SIZE];
  gsize bytes_read;
  GError *error = NULL;
  GIOStatus status;
  GIOChannel *write_to = NULL;

  if (IS_STDOUT(input_type)) {
    write_to = channel_stdin[1 ^ CLIENT_ID(input_type)];
  }

  do {
    status = g_io_channel_read_chars(source, buffer, BUFFER_SIZE,
                                     &bytes_read, &error);
    if (error != NULL) {
      stop_channels(source, write_to);
      print_error(error->message);
      return FALSE;
    }
    if (bytes_read == 0) continue;
    if (write_to != NULL) {
      g_io_channel_write_chars(write_to, buffer, bytes_read, NULL, NULL);
      g_io_channel_flush(write_to, NULL);
    }
    append_text(buffer, bytes_read, input_type);
  } while (status == G_IO_STATUS_NORMAL);

  if ((condition & ~G_IO_IN) != 0) {
    stop_channels(source, write_to);
    return FALSE;
  }
  /* for some reason I can't figure out, this function is called repeatedly
     with condition==G_IO_IN when the client process ends - this check should
     prevent the program from becoming unresponsive and consuming 100% CPU */
  return clients[CLIENT_ID(input_type)].is_running;
}

/* callback for when one of the child processes finished */
static void
child_exit_callback(GPid pid, gint status, gpointer user_data)
{
  client_t * const client = user_data;

  assert(client != NULL);
  assert(client->is_running);

  client->is_running = FALSE;
  client->status = status;
  g_spawn_close_pid(pid);
  update_status(clients);
}

void
launch_clients(const gchar *cmds[NUM_CLIENTS], GError **error)
{
  gint fd_stdin[NUM_CLIENTS];
  gint fd_stdouterr[NUM_CHANNELS];
  G_CONST_RETURN char *charset;

  assert(error == NULL || *error == NULL);

  {
    guint8 i;
    for (i = 0; i < NUM_CLIENTS; ++i) {
      gchar **cmdline;
      gboolean success;

      assert(cmds[i] != NULL);

      cmdline = g_strsplit(cmds[i], " ", 0);
      success =
        g_spawn_async_with_pipes(/* const gchar *working_directory */
                                 NULL,
                                 /* gchar **argv */
                                 cmdline,
                                 /* gchar **envp */
                                 NULL,
                                 /* GSpawnFlags flags */
                                 G_SPAWN_SEARCH_PATH |
                                 G_SPAWN_DO_NOT_REAP_CHILD,
                                 /* GSpawnChildSetupFunc child_setup */
                                 NULL,
                                 /* gpointer user_data */
                                 NULL,
                                 /* GPid *child_pid */
                                 &clients[i].pid,
                                 /* gint *standard_input */
                                 &fd_stdin[i],
                                 /* gint *standard_output */
                                 &fd_stdouterr[CHANNEL_ID(i, STDOUT)],
                                 /* gint *standard_error */
                                 &fd_stdouterr[CHANNEL_ID(i, STDERR)],
                                 /* GError **error */
                                 error);
      g_strfreev(cmdline);
      if (!success) {
        /* don't keep one client running if the other one couldn't start */
        kill_clients();
        return;
      }
      g_child_watch_add(clients[i].pid, child_exit_callback, &clients[i]);
      clients[i].is_running = TRUE;
    }
  }

  g_get_charset(&charset);

  /* open two channels for writing */
  {
    guint8 i;
    for (i = 0; i < NUM_CLIENTS; ++i) {
      channel_stdin[i] = g_io_channel_unix_new(fd_stdin[i]);
      g_io_channel_set_encoding(channel_stdin[i], charset, NULL);
    }
  }
  /* open four channels for reading, and start watching them */
  {
    guint8 i;
    for (i = 0; i < NUM_CHANNELS; ++i) {
      GIOChannel *channel;
      channel = g_io_channel_unix_new(fd_stdouterr[i]);
      g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
      g_io_channel_set_encoding(channel, charset, NULL);
      g_io_add_watch(channel, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                     io_watch_callback, GINT_TO_POINTER(i));
      g_io_channel_unref(channel);
    }
  }

  /* update the statusbar */
  update_status(clients);
}

/* send SIGTERM to any running child process */
void
kill_clients(void)
{
  guint8 i;
  for (i = 0; i < NUM_CLIENTS; ++i) {
    if (clients[i].is_running) {
      kill(clients[i].pid, SIGTERM); /* POSIX extension */
    }
  }
}
