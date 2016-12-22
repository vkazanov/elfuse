#ifndef ELFUSE_FUSE_H
#define ELFUSE_FUSE_H

#include <pthread.h>

extern pthread_mutex_t elfuse_mutex;

enum elfuse_function_waiting_enum {
    NONE,
    READY,
    READDIR,
    GETATTR,
};

extern enum elfuse_function_waiting_enum elfuse_function_waiting;

extern const char *path_arg;

/* READIR args and results */
extern char **readdir_results;
extern size_t readdir_results_size;

/* GETATTR args and results */
extern enum elfuse_getattr_result_enum {
    GETATTR_FILE,
    GETATTR_DIR,
    GETATTR_UNKNOWN,
} getattr_results;

int
elfuse_fuse_loop(char* mountpath);

void
elfuse_stop_loop();

#endif //ELFUSE_FUSE_H
