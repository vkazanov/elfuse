/* This file is part of Elfuse. */

/* Elfuse is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* Elfuse is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with Elfuse.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef ELFUSE_FUSE_H
#define ELFUSE_FUSE_H

#include <pthread.h>

extern pthread_mutex_t elfuse_mutex;
extern pthread_cond_t elfuse_cond_var;

/* Init codes */
enum elfuse_init_code_enum {
    INIT_DONE,
    INIT_ERR_ARGS,
    INIT_ERR_MOUNT,
    INIT_ERR_CREATE,
    INIT_ERR_ALLOC
} elfuse_init_code;

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

/* TRUNCATE args and results */
struct elfuse_args_truncate {
    const char *path;
    size_t size;
};

struct elfuse_results_truncate {
    enum elfuse_results_truncate_code {
        TRUNCATE_DONE,
        TRUNCATE_UNKNOWN,
    } code;
};

/* UNLINK args and results */
struct elfuse_args_unlink {
    const char *path;
};

struct elfuse_results_unlink {
    enum elfuse_results_unlink_code {
        UNLINK_DONE,
        UNLINK_UNKNOWN,
    } code;
};

/* A unified data exchange struct. */
struct elfuse_call_state {
    enum elfuse_request_state {
        /* Nothing is waiting */
        WAITING_NONE,

        /* Waiting for syscalls to be handled by Elisp */
        WAITING_CREATE,
        WAITING_RENAME,
        WAITING_GETATTR,
        WAITING_READDIR,
        WAITING_OPEN,
        WAITING_RELEASE,
        WAITING_READ,
        WAITING_WRITE,
        WAITING_TRUNCATE,
        WAITING_UNLINK,
    } request_state;

    enum elfuse_response_state {
        RESPONSE_SUCCESS,
        RESPONSE_UNDEFINED,
        RESPONSE_NOTREADY,
        RESPONSE_SIGNAL_ERROR,
        RESPONSE_UNKNOWN_ERROR,
    } response_state;
    int response_err_code;

    union args {
        struct elfuse_args_create create;
        struct elfuse_args_rename rename;
        struct elfuse_args_getattr getattr;
        struct elfuse_args_readdir readdir;
        struct elfuse_args_open open;
        struct elfuse_args_release release;
        struct elfuse_args_read read;
        struct elfuse_args_write write;
        struct elfuse_args_truncate truncate;
        struct elfuse_args_unlink unlink;
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
        struct elfuse_results_truncate truncate;
        struct elfuse_results_unlink unlink;
    } results;

} elfuse_call;

void *
elfuse_fuse_loop(void *mountpath);

#endif //ELFUSE_FUSE_H
