#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "clients.h"

#define STDERR2 0
#define STDOUT2 1
#define STDERR1 2
#define STDOUT1 3

#define IS_STDOUT(x)   (x & 1)
#define IS_STDERR(x)  ((x & 1) != 1)
#define IS_CLIENT1(x) ((x & 2) == 2)
#define IS_CLIENT2(x) ((x & 2) != 2)

void
append_text(const gchar *text, gsize len,
            gboolean is_client1, gboolean is_stdout);

void
update_status(const client_t clients[NUM_CLIENTS]);

void
print_error(gchar *message);

void
create_window_with_widgets();

#endif /* GUI_H */
