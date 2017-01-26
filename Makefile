# This file is part of Elfuse.

# Elfuse is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# Elfuse is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Elfuse.  If not, see <http://www.gnu.org/licenses/>.

CC      = gcc
LD      = gcc
CFLAGS  = -ggdb3 -Wall -Wextra -Werror -std=c11 `pkg-config fuse --cflags`
LDFLAGS = `pkg-config fuse --libs` -pthread -Wl,--no-undefined
DEPS = elfuse-fuse.h
OBJ = elfuse-module.o elfuse-fuse.o

all: elfuse-module.so

elfuse-module.so: $(OBJ)
	$(LD) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -fPIC -c $<

clean:
	rm $(OBJ)
	rm elfuse-module.so

test: elfuse-module.so
	emacs -Q -L $(PWD) --load "elfuse.el" --load "examples/write-buffer.el"

.PHONY: clean test
