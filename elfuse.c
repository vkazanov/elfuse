#include <stdbool.h>
#include <emacs-module.h>

int plugin_is_GPL_compatible;

static bool elfuse_is_started = false;

static emacs_value nil;
static emacs_value t;

static emacs_value
Felfuse_start (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
  (void)nargs; (void)args;
  if (!elfuse_is_started)
    {
      elfuse_is_started = true;

      emacs_value path_str = args[0];
      /* TODO: extract a message-printing function */
      const char success_message[] = "Fuse mounted on %s.";
      emacs_value success_message_str = env->make_string(env, success_message, sizeof(success_message) - 1);
      emacs_value message_args[] = { success_message_str, path_str };
      emacs_value message = env->intern(env, "message");
      env->funcall(env, message, 2, message_args);
      return t;
    }
  else
    {
      return nil;
    }
}

static emacs_value
Felfuse_stop (emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
  (void)env; (void)nargs; (void)args; (void)data;

  if (elfuse_is_started)
    {
      elfuse_is_started = false;
      return t;
    }
  else
    {
      return nil;
    }
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
provide (emacs_env *env, const char *feature){
  emacs_value Qfeat = env->intern (env, feature);
  emacs_value Qprovide = env->intern (env, "provide");
  emacs_value args[] = { Qfeat };

  env->funcall (env, Qprovide, 1, args);
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

  provide (env, "elfuse");

  return 0;
}
