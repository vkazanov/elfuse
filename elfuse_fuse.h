#ifndef ELFUSE_FUSE_H
#define ELFUSE_FUSE_H

int
elfuse_fuse_loop(char* mountpath);

void
elfuse_stop_loop();

#endif //ELFUSE_FUSE_H
