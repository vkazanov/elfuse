CC      = gcc
LD      = gcc
CFLAGS  = -ggdb3 -Wall `pkg-config fuse --cflags`
LDFLAGS = `pkg-config fuse --libs` -pthread -Wl,--no-undefined
DEPS = elfuse-fuse.h
OBJ = elfuse-module.o elfuse-fuse.o

TESTMOUNTPATH = mount

all: elfuse-module.so

elfuse-module.so: $(OBJ)
	$(LD) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -fPIC -c $<

clean:
	rm $(OBJ)
	rm elfuse-module.so

test: elfuse-module.so
	emacs -Q -L $(PWD) --file test.el

.PHONY: clean test
