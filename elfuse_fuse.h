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
extern enum elfuse_results_getattr_code {
    GETATTR_FILE,
    GETATTR_DIR,
    GETATTR_UNKNOWN,
} results_getattr_code;
extern size_t results_getattr_file_size;

/* READIR results */
extern char **results_readdir_files;
extern size_t results_readdir_files_size;

/* OPEN results */
extern enum elfuse_results_open_code {
    OPEN_FOUND,
    OPEN_UNKNOWN,
} results_open_code;

/* READ args and results */
extern size_t args_read_offset;
extern size_t args_read_size;
extern int read_results;
extern char *read_results_data;

int
elfuse_fuse_loop(char* mountpath);

void
elfuse_stop_loop();

#endif //ELFUSE_FUSE_H
