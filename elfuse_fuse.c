#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "elfuse_fuse.h"

void
elfuse_fuse_loop() {
    while(true) {
        fprintf(stderr, "fuse thread still working\n");
        sleep(1);
    }
}
