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

sem_t request_sem;
sem_t init_sem;
pthread_t emacs_thread;

static bool elfuse_is_started = false;
static pthread_t fuse_thread;

static emacs_value nil;
static emacs_value t;
static emacs_value elfuse_op_error;

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

static void
extract_symbol_name(emacs_env *env, emacs_value Qsymbol, char* buffer, ptrdiff_t *size)
{
    emacs_value Qsymbol_name = env->intern(env, "symbol-name");
    emacs_value args[] = {
        Qsymbol
    };
    emacs_value Sname = env->funcall(env, Qsymbol_name, 1, args);
    env->copy_string_contents(env, Sname, NULL, size);
    if (buffer != NULL) {
        env->copy_string_contents(env, Sname, buffer, size);
    }
}

static emacs_value
Felfuse_mount (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)nargs; (void)data;

    if (!elfuse_is_started) {
        emacs_value Qpath = args[0];

        ptrdiff_t buffer_length;
        env->copy_string_contents(env, Qpath, NULL, &buffer_length);
        char *path = malloc(buffer_length);
        env->copy_string_contents(env, Qpath, path, &buffer_length);

        sem_init(&request_sem, 0, 0);
        sem_init(&init_sem, 0, 0);
        if (pthread_create(&fuse_thread, NULL, elfuse_fuse_loop, path) != 0) {
            char *msg = "Elfuse: failed to launch a FUSE thread";
            message(env, msg);
            fprintf(stderr, "%s\n", msg);

            return nil;
        }

        /* Wait for initialization */
        sem_wait(&init_sem);
        sem_destroy(&init_sem);

        emacs_value res = nil;
        char *msg;
        switch (elfuse_init_code) {
        case INIT_DONE:
            elfuse_is_started = true;
            res = t;
            break;
        case INIT_ERR_ARGS:
            msg = "Elfuse: failed to launch a FUSE thread\n";
            message(env, msg);
            fprintf(stderr, "%s", msg);
            res = nil;
            break;
        case INIT_ERR_MOUNT:
            msg = "Elfuse: failed to mount on %s\n";
            message(env, msg, path);
            fprintf(stderr, "Elfuse: failed to mount on %s\n", path);
            res = nil;
            break;
        case INIT_ERR_ALLOC:
            msg = "Elfuse: failed to launch a FUSE thread";
            message(env, msg);
            fprintf(stderr, "%s", msg);
            res = nil;
            break;
        case INIT_ERR_CREATE:
            msg = "Elfuse: failed to create FUSE instance %d\n";
            message(env, msg, elfuse_init_code);
            fprintf(stderr, msg, elfuse_init_code);
            res = nil;
            break;
        default:
            msg = "Elfuse: failed to launch a FUSE thread";
            message(env, msg);
            fprintf(stderr, "%s", msg);
            res = nil;
            break;
        }

        return res;
    }
    return nil;
}

static emacs_value
Felfuse_stop (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)env; (void)nargs; (void)args; (void)data;

    if (!elfuse_is_started) {
        return nil;
    }

    elfuse_is_started = false;

    if (pthread_cancel(fuse_thread) != 0) {
        char* msg = "Elfuse: failed to cancel the FUSE thread\n";
        fprintf(stderr, "%s", msg);
        message(env, msg);
        return nil;
    }

    if (pthread_join(fuse_thread, NULL) != 0) {
        char* msg = "Elfuse: failed to join the FUSE thread\n";
        fprintf(stderr, "%s", msg);
        message(env, msg);
        return nil;
    }
    sem_destroy(&request_sem);

    return t;
}

static int handle_create(emacs_env *env, const char *path);
static int handle_rename(emacs_env *env, const char *oldpath, const char *newpath);
static int handle_readdir(emacs_env *env, const char *path);
static int handle_getattr(emacs_env *env, const char *path);
static int handle_open(emacs_env *env, const char *path);
static int handle_release(emacs_env *env, const char *path);
static int handle_read(emacs_env *env, const char *path, size_t offset, size_t size);
static int handle_write(emacs_env *env, const char *path, const char *buf, size_t size, size_t offset);
static int handle_truncate(emacs_env *env, const char *path, size_t size);
static int handle_unlink(emacs_env *env, const char *path);

static int non_local_op_exit(emacs_env *env, enum emacs_funcall_exit exit_status, emacs_value exit_symbol, emacs_value exit_data);

static emacs_value
Felfuse_check_ops(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    (void)env; (void)nargs; (void)args; (void)data;

    if (!elfuse_is_started) {
        message(env, "Elfuse loop is not running, abort.");
        return nil;
    }

    switch (elfuse_call.request_state) {
    case WAITING_CREATE:
        elfuse_call.response_state = handle_create(env, elfuse_call.args.create.path);
        break;
    case WAITING_RENAME:
        elfuse_call.response_state = handle_rename(env, elfuse_call.args.rename.oldpath, elfuse_call.args.rename.newpath);
        break;
    case WAITING_READDIR:
        elfuse_call.response_state = handle_readdir(env, elfuse_call.args.readdir.path);
        break;
    case WAITING_GETATTR:
        elfuse_call.response_state = handle_getattr(env, elfuse_call.args.getattr.path);
        break;
    case WAITING_OPEN:
        elfuse_call.response_state = handle_open(env, elfuse_call.args.open.path);
        break;
    case WAITING_RELEASE:
        elfuse_call.response_state = handle_release(env, elfuse_call.args.open.path);
        break;
    case WAITING_READ:
        elfuse_call.response_state = handle_read(
            env, elfuse_call.args.read.path, elfuse_call.args.read.offset, elfuse_call.args.read.size
        );
        break;
    case WAITING_WRITE:
        elfuse_call.response_state = handle_write(
            env, elfuse_call.args.write.path, elfuse_call.args.write.buf, elfuse_call.args.write.size, elfuse_call.args.write.offset
        );
        break;
    case WAITING_TRUNCATE:
        elfuse_call.response_state = handle_truncate(env, elfuse_call.args.truncate.path, elfuse_call.args.truncate.size);
        break;
    case WAITING_UNLINK:
        elfuse_call.response_state = handle_unlink(env, elfuse_call.args.unlink.path);
        break;
    case WAITING_NONE:
        break;
    }

    sem_post(&request_sem);

    return t;
}

static int
handle_create(emacs_env *env, const char *path)
{
    fprintf(stderr, "CREATE handle (path=%s).\n", path);

    emacs_value Qcreate = env->intern(env, "elfuse--create-op");
    if (!fboundp(env, Qcreate)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
    };
    emacs_value Ires_code = env->funcall(env, Qcreate, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    int res_code = env->extract_integer(env, Ires_code);
    elfuse_call.results.create.code = res_code >= 0 ? CREATE_DONE : CREATE_FAIL;

    return RESPONSE_SUCCESS;
}

static int
handle_rename(emacs_env *env, const char *oldpath, const char *newpath)
{
    fprintf(stderr, "RENAME handle (oldpath=%s, newpath=%s).\n", oldpath, newpath);

    emacs_value Qrename = env->intern(env, "elfuse--rename-op");
    if (!fboundp(env, Qrename)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, oldpath, strlen(oldpath)),
        env->make_string(env, newpath, strlen(newpath)),
    };
    emacs_value Ires_code = env->funcall(env, Qrename, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    int res_code = env->extract_integer(env, Ires_code);
    elfuse_call.results.rename.code = res_code >= 0 ? RENAME_DONE : RENAME_UNKNOWN;

    return RESPONSE_SUCCESS;
}

static int
handle_readdir(emacs_env *env, const char *path)
{
    fprintf(stderr, "READDIR handle (path=%s).\n", path);

    emacs_value Qreaddir = env->intern(env, "elfuse--readdir-op");
    if (!fboundp(env, Qreaddir)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value file_vector = env->funcall(env, Qreaddir, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
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
handle_getattr(emacs_env *env, const char *path)
{
    fprintf(stderr, "GETATTR handle (path=%s).\n", path);

    emacs_value Qgetattr = env->intern(env, "elfuse--getattr-op");
    if (!fboundp(env, Qgetattr)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value getattr_result_vector = env->funcall(env, Qgetattr, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
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
handle_open(emacs_env *env, const char *path)
{
    fprintf(stderr, "OPEN handle (path=%s).\n", path);

    emacs_value Qopen = env->intern(env, "elfuse--open-op");
    if (!fboundp(env, Qopen)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value Qfound = env->funcall(env, Qopen, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    if (env->eq(env, Qfound, t)) {
        elfuse_call.results.open.code = OPEN_FOUND;
    } else {
        elfuse_call.results.open.code = OPEN_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}

static int
handle_release(emacs_env *env, const char *path)
{
    fprintf(stderr, "RELEASE handle (path=%s).\n", path);

    emacs_value Qrelease = env->intern(env, "elfuse--release-op");
    if (!fboundp(env, Qrelease)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path))
    };
    emacs_value Qfound = env->funcall(env, Qrelease, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    if (env->eq(env, Qfound, t)) {
        elfuse_call.results.release.code = RELEASE_FOUND;
    } else {
        elfuse_call.results.release.code = RELEASE_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}

static int
handle_read(emacs_env *env, const char *path, size_t offset, size_t size)
{
    fprintf(stderr, "READ handle (path=%s).\n", path);

    emacs_value Qread = env->intern(env, "elfuse--read-op");
    if (!fboundp(env, Qread)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
        env->make_integer(env, offset),
        env->make_integer(env, size),
    };
    emacs_value Sdata = env->funcall(env, Qread, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    if (env->eq(env, Sdata, nil)) {
        elfuse_call.results.read.bytes_read = -1;
    } else {
        ptrdiff_t buffer_length;
        env->copy_string_contents(env, Sdata, NULL, &buffer_length);
        elfuse_call.results.read.data = malloc(buffer_length);
        if (!env->copy_string_contents(env, Sdata, elfuse_call.results.read.data, &buffer_length)) {
            elfuse_call.results.read.bytes_read = -1;
        } else {
            elfuse_call.results.read.bytes_read = buffer_length;
        }
    }

    return RESPONSE_SUCCESS;
}

static int
handle_write(emacs_env *env, const char *path, const char *buf, size_t size, size_t offset)
{
    fprintf(stderr, "WRITE handle (path=%s).\n", path);

    emacs_value Qwrite = env->intern(env, "elfuse--write-op");
    if (!fboundp(env, Qwrite)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
        env->make_string(env, buf, size),
        env->make_integer(env, offset),
    };
    emacs_value Ires_code = env->funcall(env, Qwrite, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    int res_code = env->extract_integer(env, Ires_code);
    if (res_code >= 0) {
        elfuse_call.results.write.size  = size;
    } else {
        elfuse_call.results.write.size  = res_code;
    }

    return RESPONSE_SUCCESS;
}

static int
handle_truncate(emacs_env *env, const char *path, size_t size)
{
    fprintf(stderr, "TRUNCATE handle (path=%s).\n", path);

    emacs_value Qtruncate = env->intern(env, "elfuse--truncate-op");
    if (!fboundp(env, Qtruncate)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
        env->make_integer(env, size),
    };
    emacs_value Ires_code = env->funcall(env, Qtruncate, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    if (env->extract_integer(env, Ires_code) >= 0) {
        elfuse_call.results.truncate.code  = TRUNCATE_DONE;
    } else {
        elfuse_call.results.truncate.code  = TRUNCATE_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}


static int
handle_unlink(emacs_env *env, const char *path)
{
    fprintf(stderr, "UNLINK handle (path=%s).\n", path);

    emacs_value Qunlink = env->intern(env, "elfuse--unlink-op");
    if (!fboundp(env, Qunlink)) {
        return RESPONSE_UNDEFINED;
    }

    /* Build args and execute the function call itself */
    emacs_value args[] = {
        env->make_string(env, path, strlen(path)),
    };
    emacs_value Ires_code = env->funcall(env, Qunlink, sizeof(args)/sizeof(args[0]), args);

    /* Handle possible non-local exits (signals or throws) */
    emacs_value exit_symbol, exit_data;
    enum emacs_funcall_exit exit_status = env->non_local_exit_get(
        env, &exit_symbol, &exit_data
    );
    if (exit_status != emacs_funcall_exit_return) {
        env->non_local_exit_clear(env);
        return non_local_op_exit(env, exit_status, exit_symbol, exit_data);
    }

    /* Handle proper response */
    if (env->extract_integer(env, Ires_code) >= 0) {
        elfuse_call.results.unlink.code  = UNLINK_DONE;
    } else {
        elfuse_call.results.unlink.code  = UNLINK_UNKNOWN;
    }

    return RESPONSE_SUCCESS;
}


static int
non_local_op_exit(emacs_env *env, enum emacs_funcall_exit exit_code, emacs_value exit_symbol, emacs_value exit_data)
{
    int res = RESPONSE_UNKNOWN_ERROR;
    if (exit_code == emacs_funcall_exit_signal) {
        if (env->eq(env, exit_symbol, elfuse_op_error)) {
            elfuse_call.response_err_code = env->extract_integer(env, exit_data);
            res = RESPONSE_SIGNAL_ERROR;
            fprintf(stderr, "An Elfuse signal caught (code=%d)\n", elfuse_call.response_err_code);
        } else {
            ptrdiff_t size;
            extract_symbol_name(env, exit_symbol, NULL, &size);
            char name[size];
            extract_symbol_name(env, exit_symbol, name, &size);
            fprintf(stderr, "Unknown error caught (name=%s)\n", name);
        }

    } else {
        fprintf(stderr, "An unknown non-local op exit\n");
    }
    return res;
}

int
emacs_module_init (struct emacs_runtime *ert)
{
    emacs_env *env = ert->get_environment (ert);

    nil = env->intern(env, "nil");
    t = env->intern(env, "t");
    elfuse_op_error = env->intern(env, "elfuse-op-error");
    emacs_thread = pthread_self();

    emacs_value fun = env->make_function (
        env, 1, 1,
        Felfuse_mount,
        "Start the elfuse thread. ",
        NULL
    );
    bind_function (env, "elfuse--mount", fun);

    fun = env->make_function (
        env, 0, 0,
        Felfuse_stop,
        "Kill the elfuse thread. ",
        NULL
    );
    bind_function (env, "elfuse--stop", fun);

    fun = env->make_function (
        env, 0, 0,
        Felfuse_check_ops,
        "Check if Fuse callbacks are waiting for reply and reply. ",
        NULL
    );
    bind_function (env, "elfuse--check-ops", fun);

    provide (env, "elfuse-module");

    return 0;
}
