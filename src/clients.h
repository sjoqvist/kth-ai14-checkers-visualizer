#ifndef CLIENTS_H
#define CLIENTS_H

#include <gtk/gtk.h>

#define NUM_CLIENTS 2

typedef struct {
  GPid pid;
  gboolean is_running;
  gint status;
} client_t;

void
launch_clients(const gchar *cmds[2], GError **error);

void
kill_clients(void);

#endif /* CLIENTS_H */
