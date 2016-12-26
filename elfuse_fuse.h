#ifndef ELFUSE_FUSE_H
#define ELFUSE_FUSE_H

#include <pthread.h>

extern pthread_mutex_t elfuse_mutex;
extern pthread_cond_t elfuse_cond_var;

extern enum elfuse_function_waiting_enum {
    WAITING_NONE,
    WAITING_READDIR,
    WAITING_GETATTR,
    WAITING_OPEN,
    WAITING_READ,
} elfuse_function_waiting;

extern const char *args_path;

/* GETATTR results */
extern struct elfuse_results_getattr {
    enum elfuse_results_getattr_code {
        GETATTR_FILE,
        GETATTR_DIR,
        GETATTR_UNKNOWN,
    } code;
    size_t file_size;
} results_getattr;

/* READDIR results */
extern struct elfuse_results_readdir {
    char **files;
    size_t files_size;
} results_readdir;

/* OPEN results */
extern struct elfuse_results_open {
    enum elfuse_results_open_code {
        OPEN_FOUND,
        OPEN_UNKNOWN,
    } code;
} results_open;

/* READ args and results */
extern struct elfuse_args_read {
    size_t offset;
    size_t size;
} args_read;

extern struct elfuse_results_read {
    int bytes_read;
    char *data;
} results_read;

int
elfuse_fuse_loop(char* mountpath);

void
elfuse_stop_loop();

#endif //ELFUSE_FUSE_H
