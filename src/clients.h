/*!
 * \file clients.h
 * \brief
 * Provides function prototypes for launching and killing clients
 */
#ifndef CLIENTS_H
#define CLIENTS_H

#include <gtk/gtk.h>

/*!
 * \brief
 * The number of clients to build the program for
 *
 * The only value that is currently safe to use is 2.
 */
#define NUM_CLIENTS 2

/*!
 * \brief
 * Holds information about a running or finished client
 */
typedef struct {
  /*! \brief Process id */
  GPid pid;
  /*! \brief `TRUE` until the parent has been notified of the client's exit */
  gboolean is_running;
  /*! \brief Exit status code (relevant only if #is_running is `FALSE`) */
  gint status;
} client_t;

/*!
 * \brief
 * Spawns asynchronous client processes
 *
 * \param[in] cmds   the command lines for each of the processes to be spawned
 * \param[in] error  either \c NULL to disregard errors, or the address of a
 *                   pointer initialized to \c NULL (which should be freed
 *                   afterwards if set)
 */
void
launch_clients(const gchar *cmds[static NUM_CLIENTS], GError **error);

/*!
 * \brief
 * Send SIGTERM to any running client process
 */
void
kill_clients(void);

#endif /* CLIENTS_H */
