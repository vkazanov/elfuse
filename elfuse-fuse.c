#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#include "elfuse-fuse.h"

pthread_mutex_t elfuse_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t elfuse_cond_var = PTHREAD_COND_INITIALIZER;

struct elfuse_call_state elfuse_call = {
    .request_state = WAITING_NONE
};

static int
elfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) mode;
    (void) fi;
    int res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_CREATE;

    /* Set function args */
    elfuse_call.args.create.path = path;

    /* Wait for the funcall results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    /* Got the results, see if everything's fine */
    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "CREATE callback undefined\n");
        res = -ENOSYS;
    } else {
        if (elfuse_call.results.create.code == CREATE_DONE) {
            fprintf(stderr, "CREATE received results (code=DONE)\n");
            res = 0;
        } else {
            fprintf(stderr, "CREATE received results (code=FAIL)\n");
            res = -ENOENT;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_rename(const char *oldpath, const char *newpath)
{
    int res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_RENAME;

    /* Set function args */
    elfuse_call.args.rename.oldpath = oldpath;
    elfuse_call.args.rename.newpath = newpath;

    /* Wait for the funcall results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "RENAME callback undefined\n");
        res = -ENOSYS;
    } else {
        if (elfuse_call.results.rename.code == RENAME_DONE) {
            fprintf(stderr, "RENAME received results (code=DONE)\n");
            res = 0;
        } else {
            fprintf(stderr, "RENAME received results (code=UNKNOWN)\n");
            res = -ENOENT;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_GETATTR;

    /* Set function args */
    elfuse_call.args.getattr.path = path;

    /* Wait for the funcall results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    /* Got the results, see if everything's fine */
    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "GETATTR callback undefined\n");
        res = -ENOSYS;
    } else {
        memset(stbuf, 0, sizeof(struct stat));
        if (elfuse_call.results.getattr.code == GETATTR_FILE) {
            fprintf(stderr, "GETATTR received results (file %s)\n", path);
            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 1;
            stbuf->st_size = elfuse_call.results.getattr.file_size;
            res = 0;
        } else if (elfuse_call.results.getattr.code == GETATTR_DIR) {
            fprintf(stderr, "GETATTR received results (dir %s)\n", path);
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            res = 0;
        } else {
            fprintf(stderr, "GETATTR received results (unknown %s)\n", path);
            res = -ENOENT;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    int res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_READDIR;

    /* Set function args */
    elfuse_call.args.readdir.path = path;

    /* Wait for results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    /* Got the results, see if everything's fine */
    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "READDIR callback undefined\n");
        res = -ENOSYS;
    } else {
        fprintf(stderr, "READDIR received results\n");
        for (size_t i = 0; i < elfuse_call.results.readdir.files_size; i++) {
            filler(buf, elfuse_call.results.readdir.files[i], NULL, 0);
        }

        free(elfuse_call.results.readdir.files);
        res = 0;
    }


    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_open(const char *path, struct fuse_file_info *fi)
{
    int res = 0;

    /* TODO: should be handled on the Emacs side of things */
    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_OPEN;

    /* Set callback args */
    elfuse_call.args.open.path = path;

    /* Wait for results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "OPEN callback undefined\n");
        res = -ENOSYS;
    } else {
        fprintf(stderr, "OPEN received results (%d)\n", elfuse_call.results.open.code == OPEN_FOUND);

        if (elfuse_call.results.open.code == OPEN_FOUND) {
            res = 0;
        } else {
            res = -EACCES;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_release(const char *path, struct fuse_file_info *fi)
{
    int res = 0;

    /* TODO: should be handled on the Emacs side of things */
    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_RELEASE;

    /* Set callback args */
    elfuse_call.args.release.path = path;

    /* Wait for results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "RELEASE callback undefined\n");
        res = -ENOSYS;
    } else {
        fprintf(stderr, "RELEASE received results (%d)\n", elfuse_call.results.release.code == RELEASE_FOUND);

        if (elfuse_call.results.release.code == RELEASE_FOUND) {
            res = 0;
        } else {
            res = -EACCES;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
    (void) fi;

    int res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_READ;

    /* Set function args */
    elfuse_call.args.read.path = path;
    elfuse_call.args.read.offset = offset;
    elfuse_call.args.read.size = size;

    /* Wait for the funcall results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "READ callback undefined\n");
        res = -ENOSYS;
    } else {
        if (elfuse_call.results.read.bytes_read >= 0) {
            fprintf(stderr, "READ received results %s(%d)\n", elfuse_call.results.read.data, elfuse_call.results.read.bytes_read);
            memcpy(buf, elfuse_call.results.read.data, elfuse_call.results.read.bytes_read);
            free(elfuse_call.results.read.data);
            res = elfuse_call.results.read.bytes_read;
        } else {
            fprintf(stderr, "READ did not receive results (%d)\n", elfuse_call.results.read.bytes_read);
            res = -ENOENT;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi)
{
    (void) fi;

    int res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_WRITE;

    /* Set function args */
    elfuse_call.args.write.path = path;
    elfuse_call.args.write.buf = buf;
    elfuse_call.args.write.size = size;
    elfuse_call.args.write.offset = offset;

    /* Wait for the funcall results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "WRITE callback undefined\n");
        res = -ENOSYS;
    } else {
        if (elfuse_call.results.write.size >= 0) {
            fprintf(stderr, "WRITE received results %d\n", elfuse_call.results.write.size);
            res = elfuse_call.results.write.size;
        } else {
            fprintf(stderr, "WRITE did not receive results (%d)\n", elfuse_call.results.write.size);
            res = -ENOENT;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

    return res;
}

static int
elfuse_truncate(const char *path, off_t size)
{
    size_t res = 0;

    pthread_mutex_lock(&elfuse_mutex);

    /* Function to call */
    elfuse_call.request_state = WAITING_TRUNCATE;

    /* Set function args */
    elfuse_call.args.write.path = path;
    elfuse_call.args.write.size = size;

    /* Wait for the funcall results */
    pthread_cond_wait(&elfuse_cond_var, &elfuse_mutex);

    if (elfuse_call.response_state == RESPONSE_UNDEFINED) {
        fprintf(stderr, "TRUNCATE callback undefined\n");
        res = -ENOSYS;
    } else {
        if (elfuse_call.results.truncate.code == TRUNCATE_DONE) {
            fprintf(stderr, "TRUNCATE received results %d\n", elfuse_call.results.truncate.code);
            res = 0;
        } else {
            fprintf(stderr, "TRUNCATE did not receive results (%d)\n", elfuse_call.results.truncate.code);
            res = -ENOENT;
        }
    }

    elfuse_call.request_state = WAITING_NONE;
    pthread_mutex_unlock(&elfuse_mutex);

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
};

static struct fuse *fuse;
static struct fuse_chan *ch;

static void elfuse_cleanup_args(void *args) {
    fprintf(stderr, "cleanup args\n");
    fuse_opt_free_args(args);
}

static void elfuse_cleanup_mount(void *arg) {
    fprintf(stderr, "cleanup mount\n");
    fuse_unmount(arg, ch);
}

static void elfuse_cleanup_fuse(void *arg) {
    (void) arg;

    fprintf(stderr, "cleanup fuse\n");
    fuse_destroy(fuse);
}

int
elfuse_fuse_loop(char* mountpath)
{
    int argc = 2;
    char* argv[] = {
        "",
        mountpath
    };

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    char *mountpoint;
    int err = -1;
    pthread_cleanup_push(elfuse_cleanup_args, &args);

    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
        (ch = fuse_mount(mountpoint, &args)) != NULL) {
        pthread_cleanup_push(elfuse_cleanup_mount, mountpoint);

        fuse = fuse_new(ch, &args, &elfuse_oper, sizeof(elfuse_oper), NULL);
        if (fuse != NULL) {
            pthread_cleanup_push(elfuse_cleanup_fuse, NULL);

            struct fuse_session *se = fuse_get_session(fuse);
            fprintf(stderr, "Start the Elfuse loop\n");


            /* TODO: move into a separate function with additional */
            struct fuse_chan *ch = fuse_session_next_chan(se, NULL);
            size_t bufsize = fuse_chan_bufsize(ch);
            char *buf = malloc(bufsize);
            if (!buf) {
		fprintf(stderr, "fuse: failed to allocate read buffer\n");
		return -1;
            }

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

            free(buf);
            fuse_session_reset(se);
            /* TODO: end move */

            pthread_cleanup_pop(true);
            fprintf(stderr, "Stop the Elfuse loop\n");
        }
        pthread_cleanup_pop(true);
    }
    pthread_cleanup_pop(true);

    return err ? 1 : 0;
}

void
elfuse_stop_loop()
{
    pthread_mutex_lock(&elfuse_mutex);
    bool is_looping = fuse != NULL;
    if (!is_looping) {
        pthread_mutex_unlock(&elfuse_mutex);
        return;
    }
    fuse_exit(fuse);
    pthread_mutex_unlock(&elfuse_mutex);

    while (is_looping) {
        sleep(1);
        pthread_mutex_lock(&elfuse_mutex);
        is_looping = fuse != NULL;
        pthread_mutex_unlock(&elfuse_mutex);
    }
    fprintf(stderr, "Elfuse unmounted\n");
}
