CC=c89
CFLAGS=-O2 -Wall -Wextra -pedantic
GTKINCLUDE=`pkg-config --cflags gtk+-2.0`
GTKLIBS=`pkg-config --libs gtk+-2.0`
SRCDIR=src
BUILDDIR=build
OBJDIR=$(BUILDDIR)/obj
# These are dependency templates for each .o file. The .c file must be first.
DEPS=\
 board.c:board.h \
 clients.c:clients.h:gui.h:main.h \
 gui.c:board.h:clients.h:gui.h:main.h \
 main.c:gui.h:main.h
CFILES=$(foreach dep,$(DEPS),$(firstword $(subst :, ,$(dep))))
OBJ=$(patsubst %.c,$(OBJDIR)/%.o,$(CFILES))
TARGET=$(BUILDDIR)/visualizer

.PHONY: all checkdirs strip clean

define make-goal
$(OBJDIR)/$(patsubst %.c,%.o,$(firstword $1)): $(addprefix $(SRCDIR)/,$1)
	$(CC) -o $$@ -c $(SRCDIR)/$(firstword $1) $(CFLAGS) $(GTKINCLUDE)
endef

all: checkdirs $(TARGET)

strip: all
	strip $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(CFLAGS) $(GTKLIBS)

$(foreach dep,$(DEPS),$(eval $(call make-goal,$(subst :, ,$(dep)))))

checkdirs: $(OBJDIR) $(BUILDDIR)

$(OBJDIR):
	@mkdir -p $@

$(BUILDDIR):
	@mkdir -p $@

clean:
	@rm -rf $(OBJDIR) $(BUILDDIR)
