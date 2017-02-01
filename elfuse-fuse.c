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

#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "elfuse-fuse.h"

struct elfuse_call_state elfuse_call = {
    .request_state = WAITING_NONE,
    .response_state = RESPONSE_NOTREADY,
};

static int
elfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) mode;
    (void) fi;
    int res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_CREATE;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.create.path = path;

    /* Wait for the funcall results */
    fprintf(stderr, "CREATE request (path=%s).\n", path);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    /* Got the results, see if everything's fine */
    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        fprintf(stderr, "CREATE success (code=%d)\n", elfuse_call.results.create.code);
        if (elfuse_call.results.create.code == CREATE_DONE) {
            res = 0;
        } else {
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "CREATE fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "CREATE fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "CREATE fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_rename(const char *oldpath, const char *newpath)
{
    int res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_RENAME;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.rename.oldpath = oldpath;
    elfuse_call.args.rename.newpath = newpath;

    /* Wait for the funcall results */
    fprintf(stderr, "RENAME request (oldpath=%s, newpath=%s).\n", oldpath, newpath);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        if (elfuse_call.results.rename.code == RENAME_DONE) {
            fprintf(stderr, "RENAME success (code=DONE)\n");
            res = 0;
        }  {
            fprintf(stderr, "RENAME success (code=UNKNOWN)\n");
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "RENAME fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "RENAME fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "RENAME fail unknown error\n");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_GETATTR;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.getattr.path = path;

    /* Wait for the funcall results */
    fprintf(stderr, "GETATTR request (path=%s)\n", path);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    /* Got the results, see if everything's fine */
    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        memset(stbuf, 0, sizeof(struct stat));
        if (elfuse_call.results.getattr.code == GETATTR_FILE) {
            fprintf(stderr, "GETATTR success (file %s)\n", path);
            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 1;
            stbuf->st_size = elfuse_call.results.getattr.file_size;
            res = 0;
        } else if (elfuse_call.results.getattr.code == GETATTR_DIR) {
            fprintf(stderr, "GETATTR success (dir %s)\n", path);
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            res = 0;
        } else {
            fprintf(stderr, "GETATTR success (unknown %s)\n", path);
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "GETATTR fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "GETATTR fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "GETATTR fail (unknown error)\n");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    int res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_READDIR;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.readdir.path = path;

    /* Wait for results */
    fprintf(stderr, "READDIR request (path=%s)\n", path);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    /* Got the results, see if everything's fine */
    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        size_t files_size = elfuse_call.results.readdir.files_size;
        fprintf(stderr, "READDIR success (files found = %ld)\n", files_size);
        for (size_t i = 0; i < files_size; i++) {
            filler(buf, elfuse_call.results.readdir.files[i], NULL, 0);
        }

        free(elfuse_call.results.readdir.files);
        res = 0;
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "READDIR fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "READDIR fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "READDIR fail (unknown error)\n");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_open(const char *path, struct fuse_file_info *fi)
{
    int res = 0;

    /* TODO: should be handled on the Emacs side of things */
    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    /* Function to call */
    elfuse_call.request_state = WAITING_OPEN;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set callback args */
    elfuse_call.args.open.path = path;

    /* Wait for results */
    fprintf(stderr, "OPEN request (path=%s)\n", path);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        fprintf(stderr, "OPEN success (code=%d)\n", elfuse_call.results.open.code);

        if (elfuse_call.results.open.code == OPEN_FOUND) {
            res = 0;
        } else {
            res = -EACCES;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "OPEN fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "OPEN fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "OPEN fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_release(const char *path, struct fuse_file_info *fi)
{
    int res = 0;

    /* TODO: should be handled on the Emacs side of things */
    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    /* Function to call */
    elfuse_call.request_state = WAITING_RELEASE;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set callback args */
    elfuse_call.args.release.path = path;

    /* Wait for results */
    fprintf(stderr, "RELEASE request (path=%s)\n", path);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        fprintf(stderr, "RELEASE success (code=%d)\n", elfuse_call.results.release.code);

        if (elfuse_call.results.release.code == RELEASE_FOUND) {
            res = 0;
        } else {
            res = -EACCES;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "RELEASE fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "RELEASE fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "RELEASE fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
    (void) fi;

    int res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_READ;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.read.path = path;
    elfuse_call.args.read.offset = offset;
    elfuse_call.args.read.size = size;

    /* Wait for the funcall results */
    fprintf(stderr, "READ request (path=%s, size=%ld, offset=%ld).\n", path, size, offset);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        if (elfuse_call.results.read.bytes_read >= 0) {
            fprintf(stderr, "READ success (data=%s, size=%d)\n", elfuse_call.results.read.data, elfuse_call.results.read.bytes_read);
            memcpy(buf, elfuse_call.results.read.data, elfuse_call.results.read.bytes_read);
            free(elfuse_call.results.read.data);
            res = elfuse_call.results.read.bytes_read;
        } else {
            fprintf(stderr, "READ success (no data, size=%d)\n", elfuse_call.results.read.bytes_read);
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "READ fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "READ fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "READ fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi)
{
    (void) fi;

    int res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_WRITE;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.write.path = path;
    elfuse_call.args.write.buf = buf;
    elfuse_call.args.write.size = size;
    elfuse_call.args.write.offset = offset;

    /* Wait for the funcall results */
    fprintf(stderr, "WRITE request (path=%s, size=%ld, offset=%ld).\n", path, size, offset);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        fprintf(stderr, "WRITE success (size=%d)\n", elfuse_call.results.write.size);
        if (elfuse_call.results.write.size >= 0) {
            res = elfuse_call.results.write.size;
        } else {
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "WRITE fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "WRITE fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "WRITE fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_truncate(const char *path, off_t size)
{
    size_t res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_TRUNCATE;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.write.path = path;
    elfuse_call.args.write.size = size;

    /* Wait for the funcall results */
    fprintf(stderr, "TRUNCATE request (path=%s, size=%ld).\n", path, size);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        fprintf(stderr, "TRUNCATE success (code=%d)\n", elfuse_call.results.truncate.code);
        if (elfuse_call.results.truncate.code == TRUNCATE_DONE) {
            res = 0;
        } else {
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "TRUNCATE fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "TRUNCATE fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "TRUNCATE fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;

    return res;
}

static int
elfuse_unlink(const char *path)
{
    size_t res = 0;

    /* Function to call */
    elfuse_call.request_state = WAITING_UNLINK;
    elfuse_call.response_state = RESPONSE_NOTREADY;

    /* Set function args */
    elfuse_call.args.unlink.path = path;

    /* Wait for the funcall results */
    fprintf(stderr, "UNLINK request (path=%s).\n", path);
    raise(SIGUSR1);
    sem_wait(&request_sem);

    if (elfuse_call.response_state == RESPONSE_SUCCESS) {
        fprintf(stderr, "UNLINK success (code=%d)\n", elfuse_call.results.unlink.code);
        if (elfuse_call.results.unlink.code == UNLINK_DONE) {
            res = 0;
        } else {
            res = -ENOENT;
        }
    } else if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "TRUNCATE fail (operation undefined)\n");
        res = -ENOSYS;
    } else if (elfuse_call.response_state == RESPONSE_SIGNAL_ERROR) {
        fprintf(stderr, "TRUNCATE fail (elfuse signal with errno %d)\n", elfuse_call.response_err_code);
        res = -elfuse_call.response_err_code;
    } else {
        fprintf(stderr, "TRUNCATE fail (unknown error\n)");
        res = -ENOSYS;
    }

    elfuse_call.request_state = WAITING_NONE;
    return res;
}

static struct fuse_operations elfuse_oper = {
    .create	= elfuse_create,
    .rename	= elfuse_rename,
    .getattr	= elfuse_getattr,
    .readdir	= elfuse_readdir,
    .open	= elfuse_open,
    .release	= elfuse_release,
    .read	= elfuse_read,
    .write	= elfuse_write,
    .truncate	= elfuse_truncate,
    .unlink	= elfuse_unlink,
};

static struct fuse *fuse;

static void elfuse_cleanup_mount(void *mountpoint) {
    fprintf(stderr, "Elfuse: unmounting\n");
    fuse_unmount(mountpoint, NULL);
    free(mountpoint);
}

static void elfuse_cleanup_fuse(void *buf) {
    fprintf(stderr, "Elfuse: cleanup fuse\n");
    fuse_destroy(fuse);
    free(buf);
}

void *
elfuse_fuse_loop(void* mountpath)
{
    int argc = 2;
    char* argv[] = {
        "",
        mountpath
    };


    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    char *mountpoint;
    int err = -1;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    /* Parse arguments */
    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) == -1) {
        fprintf(stderr, "Elfuse: failed parsing the command line\n");
        free(mountpath);
        free(mountpoint);

        elfuse_init_code = INIT_ERR_ARGS;
        sem_post(&init_sem);

        pthread_exit(NULL);
    }
    free(mountpath);


    /* Mount the FUSE FS */
    struct fuse_chan *ch = fuse_mount(mountpoint, &args);
    if (ch == NULL) {
        fprintf(stderr, "Elfuse: failed mounting\n");

        elfuse_init_code = INIT_ERR_MOUNT;
        sem_post(&init_sem);

        pthread_exit(NULL);
    }
    pthread_cleanup_push(elfuse_cleanup_mount, mountpoint);

    /* Create the FUSE instance */
    fuse = fuse_new(ch, &args, &elfuse_oper, sizeof(elfuse_oper), NULL);
    if (fuse == NULL) {
        fprintf(stderr, "Elfuse: failed creating FUSE\n");

        elfuse_init_code = INIT_ERR_CREATE;
        sem_post(&init_sem);

        pthread_exit(NULL);
    }
    /* Prepare a working buffer */
    size_t bufsize = fuse_chan_bufsize(ch);
    char *buf = malloc(bufsize);
    if (!buf) {
        fprintf(stderr, "Elfuse: failed to allocate the read buffer\n");

        elfuse_init_code = INIT_ERR_ALLOC;
        sem_post(&init_sem);

        pthread_exit(NULL);
    }
    pthread_cleanup_push(elfuse_cleanup_fuse, buf);

    /* Let Emacs know that init was a success */
    elfuse_init_code = INIT_DONE;
    sem_post(&init_sem);

    /* Go-go-go! */
    fprintf(stderr, "Elfuse: starting main loop\n");
    struct fuse_session *se = fuse_get_session(fuse);
    while (!fuse_session_exited(se)) {
        struct fuse_chan *tmpch = ch;
        struct fuse_buf fbuf = {
            .mem = buf,
            .size = bufsize,
        };

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        err = fuse_session_receive_buf(se, &fbuf, &tmpch);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        if (err == -EINTR)
            continue;
        if (err <= 0)
            break;

        fuse_session_process_buf(se, &fbuf, tmpch);
    }


    /* Cleanup FUSE */
    pthread_cleanup_pop(true);
    /* Cleanup the mount point */
    pthread_cleanup_pop(true);

    return NULL;
}
