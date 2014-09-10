DD2380 Artificial Intelligence (ai14) Checkers Visualizer
=========================================================
Introduction
------------
This project is meant to provide a convenient way of tracking how the
checkers-playing client that is the homework assignment 1 (HW1) in ai14
is performing. It allows the user to step forwards and backwards between
the moves, viewing a graphical representation of what's going on
together with highlighted client stdout and stderr output for the
current move. The goal is to enable the user to get a better
understanding and write a more efficient client. And of course, it's an
excuse for me to play around with GTK+.

![Screenshot of the Checkers Visualizer's main
window](https://raw.github.com/aliquis/kth-ai14-checkers-visualizer/master/doc/screenshot.png)

The program will only display what is sent to it. It doesn't know any of
the rules of the game and assumes that the current game state is valid.
I've tried to prevent it from crashing on invalid input, but it can be
made to display nonsensical game states. In particular, it doesn't use
information from past or future moves when rendering the current move,
and it would happily display the game backwards if that's what's sent to
it. **This has the implication that the source code can't provide any
unfair help by revealing algorithms useful for the homework.**

The code should work on systems running Linux, BSD or OS X. I'm also
hoping that it works in Cygwin for those running Windows, but I haven't
tested it.

Unfortunately, I can't reproduce the Checkers problem statement, as its
license is restricted and used with permission. Furthermore, the
skeleton code that is provided in the course doesn't mention a license,
and I have to assume that similar rules apply. If you've discovered this
repository at a point in time when these resources aren't available
online anymore, please have a look at the [Protocol](#protocol) section
below.

If you *do* find this project useful for whatever reason, feel free to
tell me about it and/or buy me a beer.

See also:

* [Course information](http://www.kth.se/student/kurser/kurs/DD2380?l=en)
* [Course web page](http://dd2380.csc.kth.se/)
* [Checkers problem description on
  Kattis](https://kth.kattis.scrool.se/problems/kth:ai:checkers)

Building
--------
The project has been successfully built on Arch Linux and Debian. On
Arch, make sure you have the packages `base-devel` and `gtk2` installed.
On Debian, you want `build-essential` and `libgtk2.0-dev` instead. Then
just run

```
make
```

in the project directory, alternatively

```
make strip
```

if you don't want to keep the debugging symbols.

If your version of `make` doesn't support the fancy syntax in
`Makefile`, you can achieve essentially the same thing using

```
mkdir build
c89 -o build/visualizer src/*.c -O2 `pkg-config --cflags --libs gtk+-2.0`
```

Running
-------
`make` writes the executable at `build/visualizer`. The simplest way of
running it is by skipping all arguments and just typing

```
build/visualizer
```

The executable doesn't have any dependencies in the project directory
and can be moved and installed anywhere (for example in your player
client directory). If you've done that, you can then automate the
execution using command lines like

```
./visualizer -1 './playerA init' -2 './playerB' -t 300 -r
```

This will initialize the command lines, set the animation timer at
300 milliseconds and run the clients immediately. Find out about all the
options using

```
./visualizer -h
```

Note that the command lines won't be parsed by a shell, which means that
you can't use syntax to expand arguments, or escape characters or
whitespace. The arguments must be separated by exactly one space, and
will be sent literally to the child process.

Portability
-----------
The code is written in ANSI C (C89), with the exception of the POSIX
extensions `kill(2)` and `getopt(3)`. It requires GTK+ version 2 with
header files. The project *should* thus compile and run with minimal
effort on many Unix-like operating systems. To make the project compile
on Windows, something should probably also be done about the GLib
function `g_io_channel_unix_new()`. However, it's possible that the
project will work under Cygwin. Figuring out what to do is left as an
exercise to the reader. :)

Protocol
--------
This is my interpretation (in Extended Backus-Naur Form) of the
communications protocol between the player clients, after having read
the skeleton code:

```
message     = game state, EOL ;
game state  = board, " ", description, " ", next player, " ", moves left ;
board       = 32 * 32 cell ;
cell        = empty cell | red | red king | white | white king ;
empty cell  = "." ;
red         = "r" ;
red king    = "R" ;
white       = "w" ;
white king  = "W" ;
description = static | move ;
static      = "-", ( beginning | red wins | white wins | draw | null move ) ;
beginning   = "1" ;
red wins    = "2" ;
white wins  = "3" ;
draw        = "4" ;
null move   = "5" ;
move        = normal move | jump ;
normal move = "0", waypoint, waypoint ;
jump        = "1", 2 * 2 waypoint
            | "2", 3 * 3 waypoint
            | "3", 4 * 4 waypoint
            | "4", 5 * 5 waypoint
            | "5", 6 * 6 waypoint
            | "6", 7 * 7 waypoint
            | "7", 8 * 8 waypoint
            | "8", 9 * 9 waypoint
            | "9", 10 * 10 waypoint ;
waypoint    = "_", cell id ;
cell id     = digit - "0"
            | ( "1" | "2" ), digit
            | "3", ( "0" | "1" | "2" ) ;
next player = red | white ;
moves left  = [ "1" | "2" | "3" | "4" ], digit
            | "5", "0" ;
digit       = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
```

One client is instructed to send the first message, and this client
becomes the white player. After that, the clients take turns, sending
exactly one message each until the game ends. The beginning and end of
the game are always expressed in separate messages. The final "moves
left" number represents the minimum number of moves remaining before the
game ends in a draw. Perhaps counterintuitively, each message consists
of a character telling which client *wasn't* sending the message (white
sends "`r`" and red sends "`w`"). The idea is to indicate the color of
the *next* player, but this character is also sent in end-of-game
messages.

"`W: `" and "`R: `" in the following examples are not part of the
actual messages, but added for readability. This is what the opening
11-15 would look like given the described protocol:

```
W: rrrrrrrrrrrr........wwwwwwwwwwww -1 r 50
R: rrrrrrrrrr.r..r.....wwwwwwwwwwww 0_11_15 w 49
```

The second string in the previous example can be represented as a
two-dimensional board by adding extra whitespace while maintaining the
internal order of the characters:

```
 r r r r
r r r r
 r r . r
. . r .
 . . . .
w w w w
 w w w w
w w w w
```

Here's a possible white victory:

```
W: ..WW..Ww.....R.....w............ 0_2_7 r 29
R: ..WW..Ww.R.........w............ 0_14_10 w 28
W: ..WW...w.....W.....w............ 1_7_14 r 50
R: ..WW...w.....W.....w............ -3 w 50
```

Here's what a draw might look like:

```
W: ..W..R.......................... 0_8_3 r 4
R: R.W............................. 0_6_1 w 3
W: R......W........................ 0_3_8 r 2
R: .....R.W........................ 0_1_6 w 1
W: ..W..R.......................... 0_8_3 r 0
R: ..W..R.......................... -4 w 0
```

Known issues
------------
* Under certain conditions, holding down the Ctrl or Shift key while
  selecting a row in the GtkTreeView will trick the application, as the
  widget doesn't report the correct selected row.
* The command lines don't support escape characters. The command and
  arguments are separated by exactly one space. Hence, there's currently
  no way to use space within an argument.
* There's a window resizing glitch in Awesome WM when using the grip in
  the lower right corner. `Mod4 + Button3` works as it should, as well
  as resizing in GNOME. I haven't tried it in other window managers.
* Pulling the GtkHPaned divider to its extremes will make
  `gtk_widget_size_allocate()` output a warning
  ("`attempt to allocate widget with width -XX and height YY`").
* The output buffers don't support the ANSI color codes that the
  skeleton code (provided in the course) writes. Fixing this is
  non-trivial since the buffers aren't terminals, and it's not a goal of
  mine as the visualizer is intended as a replacement for the verbose
  mode.

Lacking/TODO (may or may not get fixed)
---------------------------------------
* Overall clean-up (readability, documentation, separation of concerns).
* Analyze the code for resource leaks. I haven't seen evidence that
  prolonged use would cause for example an increase in memory usage, but
  I'd be surprised if the code is completely solid.
