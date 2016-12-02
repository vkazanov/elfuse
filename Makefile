ROOT    = /home/vladimirkazanov/var/emacs
CC      = gcc
LD      = gcc
CFLAGS  = -ggdb3 -Wall
LDFLAGS =
DEPS = elfuse_fuse.h
OBJ = elfuse.o elfuse_fuse.o

all: elfuse.so

elfuse.so: $(OBJ)
	$(LD) -shared $(LDFLAGS) -o $@ $^

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -I$(ROOT)/src -fPIC -c $<
