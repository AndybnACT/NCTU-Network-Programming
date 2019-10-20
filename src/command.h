#include <stdlib.h>
#include <sys/types.h>
#include "debug.h"

extern char **Envp;

#define STAT_READY 0x0
#define STAT_SET   0x1
#define STAT_EXEC  0x2
#define STAT_FINI  0x3
#define STAT_KILL  0x4


struct Command {
    int stat;
    pid_t pid;
    int exit_code;
    int argc;
    int (*_func)(int argc, char **argv);
    char *exec;
    char **argv;
    int fds[3];
    int pipes[2];
    char *file_out_pipe;
    struct Command *cmd_first_in_pipe;
    struct Command *cmd_out_pipe;
    struct Command *cmd_err_pipe;
    struct Command *next;
};

#define RESETCMD(cmd) {                 \
    (cmd)->pid = 0;                     \
    (cmd)->stat = STAT_READY;           \
    (cmd)->next = NULL;                 \
    (cmd)->cmd_first_in_pipe = NULL;    \
    (cmd)->cmd_out_pipe = NULL;         \
    (cmd)->cmd_err_pipe = NULL;         \
    (cmd)->pipes[0] = -1;               \
    (cmd)->pipes[1] = -1;               \
    (cmd)->fds[0] = -1;                 \
    (cmd)->fds[1] = -1;                 \
    (cmd)->fds[2] = -1;                 \
    (cmd)->argv = NULL;                 \
    (cmd)->argc = 0;                    \
    (cmd)->exec = NULL;                 \
    (cmd)->file_out_pipe = NULL;        \
}

static inline struct Command * zallocCmd(void)
{
    struct Command *ret = (struct Command*) malloc(sizeof(struct Command));
    if (!ret) {
        perror("malloc");
        exit(-1);
    }
    RESETCMD(ret);
    return ret;
}


#define dprintCmd(lvl, Cmdp) {                                                                  \
    dprintf(lvl, "self = %p\n", (Cmdp));                                                        \
    dprintf(lvl, "pid = %d\n", (Cmdp)->pid);                                                    \
    dprintf(lvl, "stat = %d\n", (Cmdp)->stat);                                                  \
    dprintf(lvl, "exit_code = %d\n", (Cmdp)->exit_code);                                        \
    if ((Cmdp)->exec){                                                                          \
        dprintf(lvl, "cmdline(%d):  ", (Cmdp)->argc);                                           \
        for (size_t i = 0; i < (Cmdp)->argc; i++)                                               \
            dprintf(lvl, "%s ", (Cmdp)->argv[i]);                                               \
        dprintf(lvl, "\n");                                                                     \
    }                                                                                           \
    dprintf(lvl, "pipe fd==> %d, %d, %d\n", (Cmdp)->fds[0], (Cmdp)->fds[1], (Cmdp)->fds[2]);     \
    dprintf(lvl, "proc in==> %p, proc out==> %p\n", (Cmdp)->cmd_first_in_pipe, (Cmdp)->cmd_out_pipe); \
    dprintf(lvl, "stdout to file %s\n" , (Cmdp)->file_out_pipe);                                \
}

/* command.c */
struct Command * parse2Cmd(char *cmdbuf, size_t bufsize, struct Command *head);

/* exec.c */
int execCmd(struct Command *Cmd);
struct Command * syncCmd(struct Command *head);

/* env.c */
#define CMD_FOUND   0x0
#define CMD_BUILTIN 0x1
#define CMD_UNKNOWN 0x2

int command_lookup(struct Command *cmdp);
int _builtin_cmd_exec(struct Command *cmdp);