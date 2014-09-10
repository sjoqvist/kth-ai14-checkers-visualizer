#ifndef CLIENTS_H
#define CLIENTS_H

#include <gtk/gtk.h>

void
launch_clients(const gchar *cmd1, const gchar *cmd2, GError **gerror);

void
kill_clients();

#endif /* CLIENTS_H */
