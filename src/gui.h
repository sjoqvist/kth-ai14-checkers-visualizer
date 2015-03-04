/*!
 * \file gui.h
 * \brief
 * Provides macros and function prototypes for tasks related to the GUI
 */
#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "clients.h"

/*!
 * \brief
 * The number of output channels (i.e. standard output and standard error for
 * each of the clients)
 */
#define NUM_CHANNELS (NUM_CLIENTS << 1)

/*! \brief Client standard output channel, for use with #CHANNEL_ID */
#define STDOUT 0
/*! \brief Client standard error channel, for use with #CHANNEL_ID */
#define STDERR 1

/*! \brief Returns true iff the channel ID refers to a stdout channel */
#define IS_STDOUT(x)  ((x & 1) == 0)
/*! \brief Returns true iff the channel ID refers to a stderr channel */
#define IS_STDERR(x)   (x & 1)
/*! \brief Returns the client ID from a channel ID */
#define CLIENT_ID(channel) ((channel) >> 1)
/*! \brief Returns the channel ID based on client ID and channel type */
#define CHANNEL_ID(client, type) (((client) << 1) | (type))

/*! \brief Limit when iterating over the dark squares in the board */
#define NUM_DARK_SQ 32

/*!
 * \brief
 * Adds incoming text to a buffer, and saves it in the store
 *
 * \param[in] text        a pointer to the incoming text
 * \param[in] len         the length of the text in bytes
 * \param[in] channel_id  an integer less than #NUM_CHANNELS, as specified by
 *                        #CHANNEL_ID, indicating where the text originates
 *                        from
 */
void
append_text(const gchar *text, gsize len, guint8 channel_id);

/*!
 * \brief
 * Updates the statusbar after a child process has spawned or exited
 *
 * \param[in] clients  the array of structs from where to read the status
 */
void
update_status(const client_t clients[static NUM_CLIENTS]);

/*!
 * \brief
 * Displays an error dialog box belonging to the main window
 *
 * \param[in] message  the message text to display
 */
void
print_error(gchar *message);

/*!
 * \brief
 * Master function to build the GUI
 */
void
create_window_with_widgets(void);

#endif /* GUI_H */
