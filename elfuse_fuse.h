#ifndef ELFUSE_FUSE_H
#define ELFUSE_FUSE_H

#include <pthread.h>

extern pthread_mutex_t elfuse_mutex;
extern pthread_cond_t elfuse_cond_var;

/* CREATE args and results */
struct elfuse_args_create {
    const char *path;
};

struct elfuse_results_create {
    enum elfuse_results_create_code {
        CREATE_FAIL,
        CREATE_DONE
    } code;
};

/* RENAME args and results */
struct elfuse_args_rename {
    const char *oldpath;
    const char *newpath;
};

struct elfuse_results_rename {
    enum elfuse_results_rename_code {
        RENAME_UNKNOWN,
        RENAME_DONE
    } code;
};

/* GETATTR args and results */
struct elfuse_args_getattr {
    const char *path;
};

struct elfuse_results_getattr {
    enum elfuse_results_getattr_code {
        GETATTR_FILE,
        GETATTR_DIR,
        GETATTR_UNKNOWN,
    } code;
    size_t file_size;
};

/* READDIR arsg and results */
struct elfuse_args_readdir {
    const char *path;
};

struct elfuse_results_readdir {
    char **files;
    size_t files_size;
};

/* OPEN args and results */
struct elfuse_args_open {
    const char *path;
};

struct elfuse_results_open {
    enum elfuse_results_open_code {
        OPEN_FOUND,
        OPEN_UNKNOWN,
    } code;
};

/* OPEN args and results */
struct elfuse_args_release {
    const char *path;
};

struct elfuse_results_release {
    enum elfuse_results_release_code {
        RELEASE_FOUND,
        RELEASE_UNKNOWN,
    } code;
};

/* READ args and results */
struct elfuse_args_read {
    const char *path;
    size_t offset;
    size_t size;
};

struct elfuse_results_read {
    int bytes_read;
    char *data;
};

/* WRITE args and results */
struct elfuse_args_write {
    const char *path;
    const char *buf;
    size_t size;
    size_t offset;
};

struct elfuse_results_write {
    int size;
};

/* A unified data exchange struct. */
struct elfuse_call_state {
    enum elfuse_state {
        WAITING_NONE,
        WAITING_CREATE,
        WAITING_RENAME,
        WAITING_GETATTR,
        WAITING_READDIR,
        WAITING_OPEN,
        WAITING_RELEASE,
        WAITING_READ,
        WAITING_WRITE,
    } state;

    union args {
        struct elfuse_args_create create;
        struct elfuse_args_rename rename;
        struct elfuse_args_getattr getattr;
        struct elfuse_args_readdir readdir;
        struct elfuse_args_open open;
        struct elfuse_args_release release;
        struct elfuse_args_read read;
        struct elfuse_args_write write;
    } args;

    union results {
        struct elfuse_results_create create;
        struct elfuse_results_rename rename;
        struct elfuse_results_getattr getattr;
        struct elfuse_results_readdir readdir;
        struct elfuse_results_open open;
        struct elfuse_results_release release;
        struct elfuse_results_read read;
        struct elfuse_results_write write;
    } results;

} elfuse_call;

int
elfuse_fuse_loop(char* mountpath);

void
elfuse_stop_loop();

#endif //ELFUSE_FUSE_H
