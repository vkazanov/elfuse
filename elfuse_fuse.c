#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include "elfuse_fuse.h"

pthread_mutex_t elfuse_mutex = PTHREAD_MUTEX_INITIALIZER;

enum elfuse_function_waiting_enum elfuse_function_waiting = NONE;

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

static int hello_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else
		res = -ENOENT;

	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

        /* TODO: Wait if busy */
        pthread_mutex_lock(&elfuse_mutex);
        elfuse_function_waiting = READDIR;
        path_arg = path;
        pthread_mutex_unlock(&elfuse_mutex);

        fprintf(stderr, "Checked - starting the loop\n");
        bool waiting = true;
        while (waiting) {
            pthread_mutex_lock(&elfuse_mutex);
            if (elfuse_function_waiting == READY) {
                fprintf(stderr, "Checked - ready\n");
                elfuse_function_waiting = NONE;
                pthread_mutex_unlock(&elfuse_mutex);
                waiting = false;
            } else if (elfuse_function_waiting == READDIR){
                pthread_mutex_unlock(&elfuse_mutex);
                fprintf(stderr, "Checked - not ready\n");
                sleep(1);
            }
        }
        fprintf(stderr, "Checked - done with the loop\n");

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, hello_path + 1, NULL, 0);

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	if(strcmp(path, hello_path) != 0)
		return -ENOENT;

	len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	} else
		size = 0;

	return size;
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

static struct fuse *f;

int
elfuse_fuse_loop(char* mountpath)
{
    int argc = 2;
    char* argv[] = {
        "",
        mountpath
    };
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    char *mountpoint;
    int err = -1;

    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
        (ch = fuse_mount(mountpoint, &args)) != NULL) {

        f = fuse_new(ch, &args, &hello_oper, sizeof(hello_oper), NULL);
        if (f != NULL) {
            fprintf(stderr, "start loop\n");
            err = fuse_loop(f);
            fprintf(stderr, "stop loop\n");
            fuse_destroy(f);
        }
        fuse_unmount(mountpoint, ch);
    }
    fuse_opt_free_args(&args);

    /* TODO: use an atomic boolean flag here? */
    pthread_mutex_lock(&elfuse_mutex);
    f = NULL;
    pthread_mutex_unlock(&elfuse_mutex);

    fprintf(stderr, "done with the loop\n");

    return err ? 1 : 0;
}

void
elfuse_stop_loop()
{
    pthread_mutex_lock(&elfuse_mutex);
    bool is_looping = f != NULL;
    if (!is_looping) {
        pthread_mutex_unlock(&elfuse_mutex);
        return;
    }
    fuse_exit(f);
    pthread_mutex_unlock(&elfuse_mutex);

    while (is_looping) {
        sleep(0.01);
        fprintf(stderr, "not yet exited\n");
        pthread_mutex_lock(&elfuse_mutex);
        is_looping = f != NULL;
        pthread_mutex_unlock(&elfuse_mutex);
    }
    fprintf(stderr, "exited\n");
}
