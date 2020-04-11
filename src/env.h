#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

struct builtin_cmd {
    char *name;
    int (*func)(int argc, char **argv);
};

int env_init();

int np_setenv(char *name, char *value, int _dummy);
char* np_getenv(char *name);


int do_printenv(int argc, char **argv);
int do_setenv(int argc, char **argv);
int do_exit(int argc, char **argv);
int do_source(int argc, char **argv);