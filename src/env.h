struct builtin_cmd {
    char *name;
    int (*func)(int argc, char **argv);
};

int do_printenv(int argc, char **argv);
int do_setenv(int argc, char **argv);
int do_exit(int argc, char **argv);
int do_source(int argc, char **argv);