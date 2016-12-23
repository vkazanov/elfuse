#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <emacs-module.h>

#include "elfuse_fuse.h"

int plugin_is_GPL_compatible;

static bool elfuse_is_started = false;
static pthread_t fuse_thread;

static emacs_value nil;
static emacs_value t;

static void *
fuse_thread_function (void *arg)
{
    elfuse_fuse_loop(arg);
    fprintf(stderr, "thread done");
    return NULL;
}

static void
message (emacs_env *env, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    size_t length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);
    va_start(ap, format);
    char buffer[length + 1];
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    emacs_value Qmessage = env->intern(env, "message");
    emacs_value args[] = {
        env->make_string(env, buffer, length)
    };
    env->funcall(env, Qmessage, sizeof(args)/sizeof(args[0]), args);
}

static void
bind_function (emacs_env *env, const char *name, emacs_value Sfun)
{
    emacs_value Qfset = env->intern (env, "fset");
    emacs_value Qsym = env->intern (env, name);
    emacs_value args[] = { Qsym, Sfun };
    env->funcall (env, Qfset, 2, args);
}

static void
provide (emacs_env *env, const char *feature)
{
    emacs_value Qfeat = env->intern (env, feature);
    emacs_value Qprovide = env->intern (env, "provide");
    emacs_value args[] = { Qfeat };
    env->funcall (env, Qprovide, 1, args);
}

static emacs_value
Felfuse_start (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)nargs; (void)data;

    if (!elfuse_is_started) {

        emacs_value Qpath = args[0];

        ptrdiff_t buffer_length;
        env->copy_string_contents(env, Qpath, NULL, &buffer_length);
        char path[buffer_length];
        env->copy_string_contents(env, Qpath, path, &buffer_length);

        if (pthread_create(&fuse_thread, NULL, fuse_thread_function, path) == 0) {
            elfuse_is_started = true;
            message(env, "FUSE thread mounted on %s", path);
            return t;
        } else {
            message(env, "Failed to init a FUSE thread");
        }
    }
    return nil;
}

static emacs_value
Felfuse_stop (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)env; (void)nargs; (void)args; (void)data;

    if (elfuse_is_started) {
        elfuse_stop_loop();
        elfuse_is_started = false;
        return t;
    }
    return nil;
}

static emacs_value
Felfuse_check_callbacks(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)env; (void)nargs; (void)args; (void)data;

    if (!elfuse_is_started) {
        message(env, "Elfuse loop is not running, abort.");
        return nil;
    }

    if (pthread_mutex_trylock(&elfuse_mutex) != 0)
        return t;

    if (elfuse_function_waiting == READDIR) {
        /* TODO: call the function required here. */
        fprintf(stderr, "Handling READDIR (path=%s).\n", path_arg);

        emacs_value args[] = {
            env->make_string(env, path_arg, strlen(path_arg))
        };
        emacs_value Qreaddir = env->intern(env, "elfuse--readdir-callback");
        emacs_value file_vector = env->funcall(env, Qreaddir, sizeof(args)/sizeof(args[0]), args);

        /* TODO: don't forget to free this later */
        readdir_results_size = env->vec_size(env, file_vector);
        readdir_results = malloc(readdir_results_size*sizeof(readdir_results[0]));

        for (size_t i = 0; i < readdir_results_size; i++) {
            emacs_value Spath = env->vec_get(env, file_vector, i);
            ptrdiff_t buffer_length;
            env->copy_string_contents(env, Spath, NULL, &buffer_length);
            char *path = malloc(buffer_length);
            env->copy_string_contents(env, Spath, path, &buffer_length);
            readdir_results[i] = path;
        }

        elfuse_function_waiting = READY;

    } else if (elfuse_function_waiting == GETATTR) {
        fprintf(stderr, "Handling GETATTR (path=%s).\n", path_arg);

        emacs_value args[] = {
            env->make_string(env, path_arg, strlen(path_arg))
        };
        emacs_value Qgetattr = env->intern(env, "elfuse--getattr-callback");

        emacs_value getattr_result_vector = env->funcall(env, Qgetattr, sizeof(args)/sizeof(args[0]), args);
        emacs_value Qfiletype = env->vec_get(env, getattr_result_vector, 0);
        emacs_value file_size = env->vec_get(env, getattr_result_vector, 1);

        if (env->eq(env, Qfiletype, env->intern(env, "file"))) {
            getattr_results = GETATTR_FILE;
            getattr_results_file_size = env->extract_integer(env, file_size);
        } else if (env->eq(env, Qfiletype, env->intern(env, "dir"))) {
            getattr_results = GETATTR_DIR;
        } else {
            getattr_results = GETATTR_UNKNOWN;
        }

        elfuse_function_waiting = READY;

    } else if (elfuse_function_waiting == OPEN) {
        fprintf(stderr, "Handling OPEN (path=%s).\n", path_arg);

        emacs_value args[] = {
            env->make_string(env, path_arg, strlen(path_arg))
        };
        emacs_value Qopen = env->intern(env, "elfuse--open-callback");
        emacs_value Qfound = env->funcall(env, Qopen, sizeof(args)/sizeof(args[0]), args);

        if (env->eq(env, Qfound, t)) {
            open_results = OPEN_FOUND;
        } else {
            open_results = OPEN_UNKNOWN;
        }

        elfuse_function_waiting = READY;

    } else if (elfuse_function_waiting == READ) {
        fprintf(stderr, "Handling READ (path=%s).\n", path_arg);

        emacs_value args[] = {
            env->make_string(env, path_arg, strlen(path_arg)),
            env->make_integer(env, read_args_offset),
            env->make_integer(env, read_args_size),
        };
        emacs_value Qread = env->intern(env, "elfuse--read-callback");
        emacs_value Sdata = env->funcall(env, Qread, sizeof(args)/sizeof(args[0]), args);

        if (env->eq(env, Sdata, nil)) {
            fprintf(stderr, "Handling READ: nil\n");
            read_results = -1;
        } else {
            ptrdiff_t buffer_length;
            env->copy_string_contents(env, Sdata, NULL, &buffer_length);
            read_results_data = malloc(buffer_length);
            if (!env->copy_string_contents(env, Sdata, read_results_data, &buffer_length)) {
                fprintf(stderr, "Failed READ: %s\n", path_arg);
                read_results = -1;
            } else {
                read_results = buffer_length;
                fprintf(stderr, "Handling READ: %s(len=%ld)\n", read_results_data, buffer_length);
            }
        }

        elfuse_function_waiting = READY;
    }

    pthread_mutex_unlock(&elfuse_mutex);

    return t;
}

int
emacs_module_init (struct emacs_runtime *ert)
{
    emacs_env *env = ert->get_environment (ert);

    nil = env->intern(env, "nil");
    t = env->intern(env, "t");

    emacs_value fun = env->make_function (
        env, 1, 1,
        Felfuse_start,
        "Start the elfuse thread. ",
        NULL
    );
    bind_function (env, "elfuse--start", fun);

    fun = env->make_function (
        env, 0, 0,
        Felfuse_stop,
        "Kill the elfuse thread. ",
        NULL
    );
    bind_function (env, "elfuse--stop", fun);

    fun = env->make_function (
        env, 0, 0,
        Felfuse_check_callbacks,
        "Check if Fuse callbacks are waiting for reply and reply. ",
        NULL
    );
    bind_function (env, "elfuse--check-callbacks", fun);

    provide (env, "elfuse");

    return 0;
}
