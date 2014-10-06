#define _POSIX_C_SOURCE 2
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "main.h"
#include "clients.h"
#include "gui.h"

/* splitting the usage string to avoid a 509 byte limitation in C90 */
static const gchar *usage =
  "Usage: %s [OPTION]...\n"
  "Visualizer for the Checkers homework assignment of the fall of 2014\n"
  "in DD2380 Artificial Intelligence (ai14) at KTH.\n"
  "\n"
  "%s%s%s"
  "Copyright (c) 2014 Anders Sj\xc3\xb6qvist <%s>\n"
  "Published under the MIT License.\n";

static const gchar *usage_execution =
  "Execution control:\n"
  "  -1 CMD   use CMD as the command line for player 1 (default \"\")\n"
  "  -2 CMD   use CMD as the command line for player 2 (default \"\")\n"
  "  -a       turn animation on (default)\n"
  "  -A       turn animation off\n"
  "  -r       run the player commands automatically after start-up\n"
  "  -R       don't run the player commands automatically (default)\n"
  "  -t NUM   set the animation timer to NUM msec (default 1000)\n"
  "\n";

static const gchar *usage_window =
  "Window control:\n"
  "  -f FONT  use FONT for the output buffers (default \"monospace 8\")\n"
  "  -m       ask the window manager to maximize the window\n"
  "  -q       quit once the animation has completed\n"
  "  -x NUM   set the window width to NUM px (default 600)\n"
  "  -y NUM   set the window height to NUM px (default 650)\n"
  "\n";

static const gchar *usage_misc =
  "Miscellaneous:\n"
  "  -h       display this help text and exit\n"
  "\n";

/* I hope this is sufficient to fool harvesters */
static gchar obfuscated_email[] = "KCTVDJ|L(*9='\x7f'#";

gchar   *default_cmds[NUM_CLIENTS] = { "", "" };
gboolean default_animate           = TRUE;
gboolean default_run               = FALSE;
guint    default_timeout           = 1000;

gchar   *default_font              = "monospace 8";
gboolean default_maximize          = FALSE;
gboolean default_quit              = FALSE;
gint     default_width             = 600;
gint     default_height            = 650;

static gboolean display_help = FALSE;

static gboolean
parse_options(int argc, char **argv)
{
  int opt;
  while((opt = getopt(argc, argv, "1:2:aAf:hmqrRt:x:y:")) != -1) {
    switch (opt) {
    case '1':
      default_cmds[0] = optarg;
      break;
    case '2':
      default_cmds[1] = optarg;
      break;
    case 'a':
      default_animate = TRUE;
      break;
    case 'A':
      default_animate = FALSE;
      break;
    case 'f':
      default_font = optarg;
      break;
    case 'h':
      display_help = TRUE;
      break;
    case 'm':
      default_maximize = TRUE;
      break;
    case 'q':
      default_quit = TRUE;
      break;
    case 'r':
      default_run = TRUE;
      break;
    case 'R':
      default_run = FALSE;
      break;
    case 't':
      sscanf(optarg, "%u", &default_timeout);
      break;
    case 'x':
      default_width = atoi(optarg);
      break;
    case 'y':
      default_height = atoi(optarg);
      break;
    default:
      display_help = TRUE;
      return FALSE;
    }
  }
  return TRUE;
}

int
main(int argc, char **argv)
{
  gboolean options_success;
  options_success = parse_options(argc, argv);

  if (display_help) {
    guint8 i;
    for (i = 0; obfuscated_email[i] != 0; ++i) {
      obfuscated_email[i] ^= 42 + 3*i;
    }
    fprintf(stderr, usage,
            argv[0], usage_execution, usage_window, usage_misc,
            obfuscated_email);
    exit(options_success ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  gtk_init(&argc, &argv);

  create_window_with_widgets();

  gtk_main();

  exit(EXIT_SUCCESS);
}
