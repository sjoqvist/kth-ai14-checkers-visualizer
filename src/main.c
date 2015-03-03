/*!
 * \file main.c
 * \brief
 * Parses the command line, prints help if necessary, sets up GTK+ and starts
 * the main event loop.
 */
/*! \cond */
#define _POSIX_C_SOURCE 2
/*! \endcond */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "main.h"
#include "clients.h"
#include "gui.h"

/*! \brief Usage message */
static const gchar *usage =
  "Usage: %s [OPTION]...\n"
  "Visualizer for the Checkers homework assignment of the fall of 2014\n"
  "in DD2380 Artificial Intelligence (ai14) at KTH.\n"
  "\n"
  "Execution control:\n"
  "  -1 CMD   use CMD as the command line for player 1 (default \"\")\n"
  "  -2 CMD   use CMD as the command line for player 2 (default \"\")\n"
  "  -a       turn animation on (default)\n"
  "  -A       turn animation off\n"
  "  -r       run the player commands automatically after start-up\n"
  "  -R       don't run the player commands automatically (default)\n"
  "  -t NUM   set the animation timer to NUM msec (default 1000)\n"
  "\n"
  "Window control:\n"
  "  -f FONT  use FONT for the output buffers (default \"monospace 8\")\n"
  "  -m       ask the window manager to maximize the window\n"
  "  -q       quit once the animation has completed\n"
  "  -x NUM   set the window width to NUM px (default 600)\n"
  "  -y NUM   set the window height to NUM px (default 650)\n"
  "\n"
  "Miscellaneous:\n"
  "  -h       display this help text and exit\n"
  "\n"
  "Copyright (c) 2014 Anders Sj\xc3\xb6qvist <%s>\n"
  "Published under the MIT License.\n";

/*! \brief Author's obfuscated e-mail address to fool harvesters */
static gchar obfuscated_email[] = "KCTVDJ|L(*9='\x7f'#";

/*! \brief Initial strings for the command lines */
gchar   *option_cmds[NUM_CLIENTS];
/*! \brief Initial state of the Animate button */
gboolean option_animate           = TRUE;
/*!
 * \brief
 * If set to \c TRUE, a Run button click is made automatically after the
 * program starts
 */
gboolean option_run               = FALSE;
/*! \brief Time spent on each animation step in milliseconds */
guint    option_timeout_ms        = 1000;

/*! \brief Font for the output buffer textviews */
gchar   *option_font              = "monospace 8";
/*! \brief If set to \c TRUE, initially maximize the window */
gboolean option_maximize          = FALSE;
/*!
 * \brief
 * If set to \c TRUE, quit the program after the animation reaches the end
 */
gboolean option_quit              = FALSE;
/*! \brief Initial window width in pixels */
gint     option_width_px          = 600;
/*! \brief Initial window height in pixels */
gint     option_height_px         = 650;

/*!
 * \brief
 * Parses the command line options and updates global variables
 *
 * \param[in]  argc         the number of options
 * \param[in]  argv         an array of options
 * \param[out] display_help set to \c TRUE if the caller should display the
 *                          help section (the pointer must not be \c NULL and
 *                          the variable must be initialized to \c FALSE)
 * \return whether the options were successfully read (i.e. whether their
 *         syntax was correct)
 */
static gboolean
parse_options(const int        argc,
              char * const    *argv,
              gboolean * const display_help)
{
  int opt;

  assert(display_help != NULL);
  assert(*display_help == FALSE);

  while((opt = getopt(argc, argv, "1:2:aAf:hmqrRt:x:y:")) != -1) {
    switch (opt) {
    case '1':
      option_cmds[0] = optarg;
      break;
    case '2':
      option_cmds[1] = optarg;
      break;
    case 'a':
      option_animate = TRUE;
      break;
    case 'A':
      option_animate = FALSE;
      break;
    case 'f':
      option_font = optarg;
      break;
    case 'h':
      *display_help = TRUE;
      break;
    case 'm':
      option_maximize = TRUE;
      break;
    case 'q':
      option_quit = TRUE;
      break;
    case 'r':
      option_run = TRUE;
      break;
    case 'R':
      option_run = FALSE;
      break;
    case 't':
      sscanf(optarg, "%u", &option_timeout_ms);
      break;
    case 'x':
      option_width_px = atoi(optarg);
      break;
    case 'y':
      option_height_px = atoi(optarg);
      break;
    default:
      *display_help = TRUE;
      return FALSE;
    }
  }
  return TRUE;
}

/*!
 * \brief
 * Program entry point.
 */
int
main(int argc, char **argv)
{
  gboolean options_success;
  gboolean display_help = FALSE;
  options_success = parse_options(argc, argv, &display_help);

  if (display_help) {
    for (guint8 i = 0; obfuscated_email[i] != 0; ++i) {
      obfuscated_email[i] ^= 42 + 3*i;
    }
    fprintf(stderr, usage, argv[0], obfuscated_email);
    exit(options_success ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  gtk_init(&argc, &argv);

  create_window_with_widgets();

  gtk_main();

  exit(EXIT_SUCCESS);
}
