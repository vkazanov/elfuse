#ifndef ELFUSE_FUSE_H
#define ELFUSE_FUSE_H

#include <pthread.h>

extern pthread_mutex_t elfuse_mutex;

enum elfuse_function_waiting_enum {
    NONE,
    READY,
    READDIR,
};

extern enum elfuse_function_waiting_enum elfuse_function_waiting;

extern const char *path_arg;
extern char **path_results;
extern size_t path_results_size;

int
elfuse_fuse_loop(char* mountpath);

void
elfuse_stop_loop();

#endif //ELFUSE_FUSE_H
