ROOT    = /home/vladimirkazanov/var/emacs
CC      = gcc
LD      = gcc
CFLAGS  = -ggdb3 -Wall
LDFLAGS =

all: elfuse.so

%.so: %.o
	$(LD) -shared $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -I$(ROOT)/src -fPIC -c $<
