#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "clients.h"

#define NUM_CHANNELS (NUM_CLIENTS << 1)

#define STDOUT 0
#define STDERR 1

#define IS_STDOUT(x)  ((x & 1) == 0)
#define IS_STDERR(x)   (x & 1)
#define CLIENT_ID(channel) ((channel) >> 1)
#define CHANNEL_ID(client, type) (((client) << 1) | (type))

void
append_text(const gchar *text, gsize len, guint8 channel_type);

void
update_status(const client_t clients[NUM_CLIENTS]);

void
print_error(gchar *message);

void
create_window_with_widgets();

#endif /* GUI_H */
