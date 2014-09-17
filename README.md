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

The code works on at least Windows, OS X and Linux. Below, you'll find
[instructions on how to build the project](#building) on the different
operating systems.

The program will only display what is sent to it. It doesn't know any of
the rules of the game and assumes that the current game state is valid.
I've tried to prevent it from crashing on invalid input, but it can be
made to display nonsensical game states. In particular, it doesn't use
information from past or future moves when rendering the current move,
and it would happily display the game backwards if that's what's sent to
it. **This has the implication that the source code can't provide any
unfair help by revealing algorithms useful for the homework.**

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
Start by downloading or cloning the repository, and read the
instructions on how to prepare your particular operating system. Then
proceed to [General](#general).

### Windows ###
Although the program doesn't make use of the Windows-specific parts of
GTK+, it compiles and runs well under Cygwin (tested on Windows 8).

Install [Cygwin](https://www.cygwin.com/) from its web page, and mark
these packages for installation while you're at it:

* `gcc-core`
* `libgtk2.0-devel`
* `make`
* `pkg-config`
* `xinit`
* `xorg-server`

Those are enough to compile the Visualizer, but you might want other
packages as well (such as `gcc-g++` if you're writing your checkers
player in C++).

Run

```
startxwin
```

While you can compile the project without having started Cygwin/X,
you'll get the message `Gtk-WARNING **: cannot open display:` if you
forget to start it before trying to run the application. Also remember
that your executable will be written to `build/visualizer.exe` (not
`build/visualizer` as in OS X or Linux).

### OS X ###
The code compiles and runs well on OS X Mavericks (version 10.9).
But there are differences between the versions and your mileage may
vary. Also, you're likely to receive some warnings which you can ignore.

First, install the Xcode command line tools. While waiting for Xcode to
download, decide whether you want to use Homebrew or JHBuild. Homebrew
makes the executable dependent on XQuartz.

#### Homebrew ####
Install [XQuartz](http://xquartz.macosforge.org/) (otherwise you'll get
stuck at the GTK+ step below), log out and log back in. Then go to
[Homebrew's website](http://brew.sh/) and run their Ruby install script.
Then:

```
brew install pkg-config
brew install gtk+
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/X11/lib/pkgconfig
```

#### JHBuild and GTK-OSX ####
Go to [the GTK-OSX
website](https://wiki.gnome.org/Projects/GTK%2B/OSX/Building#Procedure)
and go through the steps listed under "Procedure".

Don't forget that you need to start a jhbuild shell before you can
compile the code:

```
jhbuild shell
```

### Linux ###
The project was developed on Arch Linux and has been tested on Debian
"Wheezy".

#### Arch Linux ####
Install the packages `base-devel` and `gtk2`.

#### Debian ####
Install the packages `build-essential` and `libgtk2.0-dev`.

#### Other distributions ####
It should be trivial to deduct what to do from the examples above.
Basically, you want a C compiler, GNU Make, pkg-config and the header
files for GTK+ version 2.

### General ###
Once you've met all the requirements listed above, just run

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
header files. In theory, it should run on a bunch of other operating
systems as well, apart from the ones listed above. Let me know if you
attempt it.

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
* Resize and redraw operations are running with a lower priority than
  other events. As a consequence, if a player client continuously
  outputs data (for example because it's stuck in an infinite loop),
  this will make the Visualizer appear to be frozen, although that's not
  necessarily the case.

Lacking/TODO (may or may not get fixed)
---------------------------------------
* Overall clean-up (readability, documentation, separation of concerns).
* Analyze the code for resource leaks. I haven't seen evidence that
  prolonged use would cause for example an increase in memory usage, but
  I'd be surprised if the code is completely solid.
