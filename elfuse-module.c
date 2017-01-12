#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include "emacs-module.h"
#include "elfuse-fuse.h"

int plugin_is_GPL_compatible;

static bool elfuse_is_started = false;
static pthread_t fuse_thread;

static emacs_value nil;
static emacs_value t;

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

static bool
fboundp (emacs_env *env, emacs_value Sfun) {
    emacs_value Qfboundp = env->intern (env, "fboundp");
    emacs_value args[] = { Sfun };
    return env->is_not_nil(env, env->funcall (env, Qfboundp, 1, args));
}

static emacs_value
Felfuse_start (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)nargs; (void)data;

    if (!elfuse_is_started) {
        fprintf(stderr, "Trying to start the FUSE thread\n");
        emacs_value Qpath = args[0];

        ptrdiff_t buffer_length;
        env->copy_string_contents(env, Qpath, NULL, &buffer_length);
        char *path = malloc(buffer_length);
        env->copy_string_contents(env, Qpath, path, &buffer_length);

        fprintf(stderr, "Creating the FUSE thread\n");
        if (pthread_create(&fuse_thread, NULL, elfuse_fuse_loop, path) == 0) {
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
        elfuse_is_started = false;
        if (pthread_cancel(fuse_thread) != 0) {
            fprintf(stderr, "Failed to cancel the FUSE thread\n");
            return nil;
        }
        if (pthread_join(fuse_thread, NULL) != 0) {
            fprintf(stderr, "Failed to join the FUSE thread\n");
            return nil;
        }
        fprintf(stderr, "FUSE thread cancelled\n");
        return t;
    }
    return nil;
}

static int elfuse_handle_create(emacs_env *env, const char *path);
static int elfuse_handle_rename(emacs_env *env, const char *oldpath, const char *newpath);
static int elfuse_handle_readdir(emacs_env *env, const char *path);
static int elfuse_handle_getattr(emacs_env *env, const char *path);
static int elfuse_handle_open(emacs_env *env, const char *path);
static int elfuse_handle_release(emacs_env *env, const char *path);
static int elfuse_handle_read(emacs_env *env, const char *path, size_t offset, size_t size);
static int elfuse_handle_write(emacs_env *env, const char *path, const char *buf, size_t size, size_t offset);
static int elfuse_handle_truncate(emacs_env *env, const char *path, size_t size);

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

    switch (elfuse_call.request_state) {
    case WAITING_CREATE:
        elfuse_call.response_state = elfuse_handle_create(env, elfuse_call.args.create.path);
        break;
    case WAITING_RENAME:
        elfuse_call.response_state = elfuse_handle_rename(env, elfuse_call.args.rename.oldpath, elfuse_call.args.rename.newpath);
        break;
    case WAITING_READDIR:
        elfuse_call.response_state = elfuse_handle_readdir(env, elfuse_call.args.readdir.path);
        break;
    case WAITING_GETATTR:
        elfuse_call.response_state = elfuse_handle_getattr(env, elfuse_call.args.getattr.path);
        break;
    case WAITING_OPEN:
        elfuse_call.response_state = elfuse_handle_open(env, elfuse_call.args.open.path);
        break;
    case WAITING_RELEASE:
        elfuse_call.response_state = elfuse_handle_release(env, elfuse_call.args.open.path);
        break;
    case WAITING_READ:
        elfuse_call.response_state = elfuse_handle_read(
            env, elfuse_call.args.read.path, elfuse_call.args.read.offset, elfuse_call.args.read.size
        );
        break;
    case WAITING_WRITE:
        elfuse_call.response_state = elfuse_handle_write(
            env, elfuse_call.args.write.path, elfuse_call.args.write.buf, elfuse_call.args.write.size, elfuse_call.args.write.offset
        );
        break;
    case WAITING_TRUNCATE:
        elfuse_call.response_state = elfuse_handle_truncate(env, elfuse_call.args.truncate.path, elfuse_call.args.truncate.size);
        break;
    case WAITING_NONE:
        break;
    }

    pthread_cond_signal(&elfuse_cond_var);
    pthread_mutex_unlock(&elfuse_mutex);

    return t;
}

static int
elfuse_handle_create(emacs_env *env, const char *path)
{
    fprintf(stderr, "Handling CREATE (path=%s).\n", path);

    emacs_value Qcreate = env->intern(env, "elfuse--create-callback");
    if (!fboundp(env, Qcreate)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
    };
    emacs_value Ires_code = env->funcall(env, Qcreate, sizeof(args)/sizeof(args[0]), args);
    int res_code = env->extract_integer(env, Ires_code);
    fprintf(stderr, "CREATE result = %d\n", res_code);
    elfuse_call.results.create.code = res_code >= 0 ? CREATE_DONE : CREATE_FAIL;

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_rename(emacs_env *env, const char *oldpath, const char *newpath)
{
    fprintf(stderr, "Handling RENAME (oldpath=%s, newpath=%s).\n", oldpath, newpath);

    emacs_value Qrename = env->intern(env, "elfuse--rename-callback");
    if (!fboundp(env, Qrename)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, oldpath, strlen(oldpath)),
        env->make_string(env, newpath, strlen(newpath)),
    };
    emacs_value Ires_code = env->funcall(env, Qrename, sizeof(args)/sizeof(args[0]), args);
    int res_code = env->extract_integer(env, Ires_code);
    fprintf(stderr, "RENAME result = %d\n", res_code);
    elfuse_call.results.rename.code = res_code >= 0 ? RENAME_DONE : RENAME_UNKNOWN;

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_readdir(emacs_env *env, const char *path)
{
    fprintf(stderr, "Handling READDIR (path=%s).\n", path);

    emacs_value Qreaddir = env->intern(env, "elfuse--readdir-callback");
    if (!fboundp(env, Qreaddir)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value file_vector = env->funcall(env, Qreaddir, sizeof(args)/sizeof(args[0]), args);

    elfuse_call.results.readdir.files_size = env->vec_size(env, file_vector);
    size_t arr_bytes_length = elfuse_call.results.readdir.files_size*sizeof(elfuse_call.results.readdir.files[0]);
    elfuse_call.results.readdir.files = malloc(arr_bytes_length);

    for (size_t i = 0; i < elfuse_call.results.readdir.files_size; i++) {
        emacs_value Spath = env->vec_get(env, file_vector, i);
        ptrdiff_t buffer_length;
        env->copy_string_contents(env, Spath, NULL, &buffer_length);
        char *dirpath = malloc(buffer_length);
        env->copy_string_contents(env, Spath, dirpath, &buffer_length);
        elfuse_call.results.readdir.files[i] = dirpath;
    }

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_getattr(emacs_env *env, const char *path)
{
    fprintf(stderr, "Handling GETATTR (path=%s).\n", path);

    emacs_value Qgetattr = env->intern(env, "elfuse--getattr-callback");
    if (!fboundp(env, Qgetattr)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value getattr_result_vector = env->funcall(env, Qgetattr, sizeof(args)/sizeof(args[0]), args);
    emacs_value Qfiletype = env->vec_get(env, getattr_result_vector, 0);
    emacs_value file_size = env->vec_get(env, getattr_result_vector, 1);

    if (env->eq(env, Qfiletype, env->intern(env, "file"))) {
        elfuse_call.results.getattr.code = GETATTR_FILE;
        elfuse_call.results.getattr.file_size = env->extract_integer(env, file_size);
    } else if (env->eq(env, Qfiletype, env->intern(env, "dir"))) {
        elfuse_call.results.getattr.code = GETATTR_DIR;
    } else {
        elfuse_call.results.getattr.code = GETATTR_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_open(emacs_env *env, const char *path)
{
    fprintf(stderr, "Handling OPEN (path=%s).\n", path);

    emacs_value Qopen = env->intern(env, "elfuse--open-callback");
    if (!fboundp(env, Qopen)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value Qfound = env->funcall(env, Qopen, sizeof(args)/sizeof(args[0]), args);

    if (env->eq(env, Qfound, t)) {
        elfuse_call.results.open.code = OPEN_FOUND;
    } else {
        elfuse_call.results.open.code = OPEN_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_release(emacs_env *env, const char *path)
{
    fprintf(stderr, "Handling RELEASE (path=%s).\n", path);

    emacs_value Qrelease = env->intern(env, "elfuse--release-callback");
    if (!fboundp(env, Qrelease)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value Qfound = env->funcall(env, Qrelease, sizeof(args)/sizeof(args[0]), args);

    if (env->eq(env, Qfound, t)) {
        elfuse_call.results.release.code = RELEASE_FOUND;
    } else {
        elfuse_call.results.release.code = RELEASE_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_read(emacs_env *env, const char *path, size_t offset, size_t size)
{
    fprintf(stderr, "Handling READ (path=%s).\n", path);

    emacs_value Qread = env->intern(env, "elfuse--read-callback");
    if (!fboundp(env, Qread)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
        env->make_integer(env, offset),
        env->make_integer(env, size),
    };
    emacs_value Sdata = env->funcall(env, Qread, sizeof(args)/sizeof(args[0]), args);

    if (env->eq(env, Sdata, nil)) {
        fprintf(stderr, "Handling READ: nil\n");
        elfuse_call.results.read.bytes_read = -1;
    } else {
        ptrdiff_t buffer_length;
        env->copy_string_contents(env, Sdata, NULL, &buffer_length);
        elfuse_call.results.read.data = malloc(buffer_length);
        if (!env->copy_string_contents(env, Sdata, elfuse_call.results.read.data, &buffer_length)) {
            fprintf(stderr, "Failed READ: %s\n", path);
            elfuse_call.results.read.bytes_read = -1;
        } else {
            elfuse_call.results.read.bytes_read = buffer_length;
            fprintf(stderr, "Handling READ: %s(len=%ld)\n", elfuse_call.results.read.data, buffer_length);
        }
    }

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_write(emacs_env *env, const char *path, const char *buf, size_t size, size_t offset)
{
    fprintf(stderr, "Handling WRITE (path=%s).\n", path);

    emacs_value Qwrite = env->intern(env, "elfuse--write-callback");
    if (!fboundp(env, Qwrite)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
        env->make_string(env, buf, size),
        env->make_integer(env, offset),
    };
    emacs_value Ires_code = env->funcall(env, Qwrite, sizeof(args)/sizeof(args[0]), args);
    int res_code = env->extract_integer(env, Ires_code);
    if (res_code >= 0) {
        elfuse_call.results.write.size  = size;
    } else {
        elfuse_call.results.write.size  = res_code;
    }

    return RESPONSE_SUCCESS;
}

static int
elfuse_handle_truncate(emacs_env *env, const char *path, size_t size)
{
    fprintf(stderr, "Handling TRUNCATE (path=%s).\n", path);

    emacs_value Qtruncate = env->intern(env, "elfuse--truncate-callback");
    if (!fboundp(env, Qtruncate)) {
        return RESPONSE_UNDEFINED;
    }

    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
        env->make_integer(env, size),
    };

    emacs_value Ires_code = env->funcall(env, Qtruncate, sizeof(args)/sizeof(args[0]), args);
    if (env->extract_integer(env, Ires_code) >= 0) {
        elfuse_call.results.truncate.code  = TRUNCATE_DONE;
    } else {
        elfuse_call.results.truncate.code  = TRUNCATE_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
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

    provide (env, "elfuse-module");

    return 0;
}
