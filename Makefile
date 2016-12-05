EMACSSRC    = /home/vladimirkazanov/var/emacs
CC      = gcc
LD      = gcc
CFLAGS  = -ggdb3 -Wall `pkg-config fuse --cflags`
LDFLAGS = `pkg-config fuse --libs` -pthread -Wl,--no-undefined
DEPS = elfuse_fuse.h
OBJ = elfuse.o elfuse_fuse.o

all: elfuse.so

elfuse.so: $(OBJ)
	$(LD) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -I$(EMACSSRC)/src -fPIC -c $<

clean:
	rm $(OBJ)
	rm elfuse.so
