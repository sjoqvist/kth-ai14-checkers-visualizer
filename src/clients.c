#define _POSIX_C_SOURCE 1
#include <gtk/gtk.h>
#include <sys/types.h>
#include <signal.h>
#include "clients.h"
#include "main.h"
#include "gui.h"

static GIOChannel *channel_stdin[2] = { NULL, NULL };

typedef struct {
  GPid pid;
  gboolean is_running;
  gint status;
} client_t;

static client_t clients[2] = {
  { 0, FALSE, 0 },
  { 0, FALSE, 0 }
};

/* callback for when new data as available in a pipe */
static gboolean
io_watch_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
  const gint input_type = GPOINTER_TO_INT(data);
  gchar buf[1024];
  gsize bytes_read;
  GError *error = NULL;
  GIOStatus status;
  GIOChannel *write_to = NULL;

  if (IS_STDOUT(input_type)) {
    write_to = channel_stdin[STDOUT1 == input_type];
  }

  do {
    status = g_io_channel_read_chars(source, buf, sizeof(buf)/sizeof(gchar),
                                     &bytes_read, &error);
    if (error != NULL) {
      g_io_channel_shutdown(source, FALSE, NULL);
      if (write_to != NULL) {
        g_io_channel_shutdown(write_to, FALSE, NULL);
        g_io_channel_unref(write_to);
      }
      print_error(error->message);
      return FALSE;
    }
    if (bytes_read == 0) {
      continue;
    }
    if (write_to != NULL) {
      g_io_channel_write_chars(write_to, buf, bytes_read, NULL, NULL);
      g_io_channel_flush(write_to, NULL);
    }
    append_text(buf, bytes_read,
                IS_CLIENT1(input_type), IS_STDOUT(input_type));
  } while (status == G_IO_STATUS_NORMAL);

  if ((condition & ~G_IO_IN) != 0) {
    g_io_channel_shutdown(source, FALSE, NULL);
    if (write_to != NULL) {
      g_io_channel_shutdown(write_to, FALSE, NULL);
      g_io_channel_unref(write_to);
    }
    return FALSE;
  }
  /* for some reason I can't figure out, this function is called repeatedly
     with condition==G_IO_IN when the client process ends - this check should
     prevent the program from becoming unresponsive and consuming 100% CPU */
  return clients[!IS_CLIENT1(input_type)].is_running;
}

/* callback for when one of the child processes finished */
static void
child_exit_callback(GPid pid, gint status, gpointer user_data)
{
  unsigned i;

  UNUSED(user_data);

  for (i = 0; i < sizeof(clients)/sizeof(clients[0]); ++i) {
    if (pid == clients[i].pid) {
      clients[i].is_running = FALSE;
      clients[i].status = status;
      break;
    }
  }
  g_spawn_close_pid(pid);
  update_status(clients[0].pid, clients[0].is_running, clients[0].status,
                clients[1].pid, clients[1].is_running, clients[1].status);
}

void
launch_clients(const gchar *cmd1, const gchar *cmd2, GError **gerror)
{
  GError *error = NULL;
  gchar **cmdline;
  gint fd_stdin[2];
  gint fd_stdouterr[4];
  gboolean success;
  G_CONST_RETURN char *charset;

  cmdline = g_strsplit(cmd1, " ", 0);
  success = g_spawn_async_with_pipes(NULL,
                                     cmdline,
                                     NULL,
                                     G_SPAWN_SEARCH_PATH |
                                     G_SPAWN_DO_NOT_REAP_CHILD,
                                     NULL,
                                     NULL,
                                     &clients[0].pid,
                                     &fd_stdin[0],
                                     &fd_stdouterr[STDOUT1],
                                     &fd_stdouterr[STDERR1],
                                     &error);
  g_strfreev(cmdline);
  if (!success) {
    if (gerror != NULL) {
      *gerror = error;
    } else {
      g_error_free(error);
    }
    return;
  }
  g_child_watch_add(clients[0].pid, child_exit_callback, NULL);
  clients[0].is_running = TRUE;

  cmdline = g_strsplit(cmd2, " ", 0);
  success = g_spawn_async_with_pipes(NULL,
                                     cmdline,
                                     NULL,
                                     G_SPAWN_SEARCH_PATH |
                                     G_SPAWN_DO_NOT_REAP_CHILD,
                                     NULL,
                                     NULL,
                                     &clients[1].pid,
                                     &fd_stdin[1],
                                     &fd_stdouterr[STDOUT2],
                                     &fd_stdouterr[STDERR2],
                                     &error);
  g_strfreev(cmdline);
  if (!success) {
    if (gerror != NULL) {
      *gerror = error;
    } else {
      g_error_free(error);
    }
    kill(clients[0].pid, SIGTERM); /* POSIX extension */
    return;
  }
  g_child_watch_add(clients[1].pid, child_exit_callback, NULL);
  clients[1].is_running = TRUE;

  g_get_charset(&charset);

  /* open two channels for writing */
  {
    int i;
    for (i = 0; i < 2; ++i) {
      channel_stdin[i] = g_io_channel_unix_new(fd_stdin[i]);
      g_io_channel_set_encoding(channel_stdin[i], charset, NULL);
    }
  }
  /* open four channels for reading, and start watching them */
  {
    int i;
    for (i = 0; i < 4; ++i) {
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
  update_status(clients[0].pid, clients[0].is_running, clients[0].status,
                clients[1].pid, clients[1].is_running, clients[1].status);
}

/* send SIGTERM to any running child process */
void
kill_clients()
{
  unsigned i;
  for (i = 0; i < sizeof(clients)/sizeof(clients[0]); ++i) {
    if (clients[i].is_running) {
      kill(clients[i].pid, SIGTERM); /* POSIX extension */
    }
  }
}
