#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "command.h"
#include "env.h"
#include "net.h"

struct builtin_cmd Builtin_Cmds[] = {
    {"printenv", do_printenv},
    {"setenv", do_setenv},
    {"exit", do_exit},
    {"source", do_source},
    {"who", npclient_who},
    {"name", npclient_name},
    {"tell", npclient_tell},
    {"yell", npclient_yell}
};
const int NCMD = (sizeof(Builtin_Cmds)/sizeof(struct builtin_cmd));

static inline char * _copy_path(void)
{
    char *path;
    path = getenv("PATH");
    if (!path) {
        printf("Error, cound not find $PATH\n");
        exit(-1);
    }
    path = strdup(path);
    if (!path) {
        perror("strdup");
        exit(-1);
    }
    return path;
}

int command_lookup(struct Command *cmdp)
{
    int rc = 0;
    int nrpth;
    int found = 0;
    int bufsize = 0;
    int pthsize = 0;
    char *lookupbuf;
    char *path, *path_ptr;
    char *cmdname = cmdp->exec;
    
    // check if cmd is builtin command
    for (size_t i = 0; i < NCMD; i++) {
        if (!strcmp(cmdname, Builtin_Cmds[i].name)) {
            if (Builtin_Cmds[i].func == do_source) {
                cmdp->source = 1;
                cmdname = "npshell";
                break;
            }
            cmdp->_func = Builtin_Cmds[i].func;
            return -CMD_BUILTIN;
        }
    }
    
    // lookup PATH
    path = _copy_path();
    pthsize = strlen(path);
    bufsize = pthsize + strlen(cmdname) + 1; // +1 for PATH'/'cmdname
    lookupbuf = (char*) malloc(bufsize);
    memset(lookupbuf, 0, bufsize);

    nrpth = 1;
    for (size_t i = 0; i < pthsize; i++){
        if (path[i] == ':'){
            path[i] = '\0';
            nrpth++;
        }
    }
    
    path_ptr = path;
    for (size_t i = 0; i < nrpth; i++) {
        int len = strlen(path_ptr);
        strcpy(lookupbuf, path_ptr);
        lookupbuf[len] = '/';
        strcpy(&lookupbuf[len+1], cmdname);
        dprintf(2, "looking up %s (%zu, %d)\n", lookupbuf, i, nrpth);
        rc = access(lookupbuf, F_OK);
        if (rc == 0) {
            found = 1;
            break;
        }
        path_ptr = &path_ptr[len+1];
    }
    
    if (found) {
        free(cmdp->exec);
        cmdp->exec = lookupbuf;
        rc = CMD_FOUND;
    }else{
        free(lookupbuf);
        rc = -CMD_UNKNOWN;
    }
    free(path);
    return rc;
}

// must be called after command_lookup
int _builtin_cmd_exec(struct Command *cmdp)
{
    if (!cmdp->_func)
        fprintf(stdout, "BUG!! builtin function = NULL for cmd %s\n", cmdp->exec);
    
    return cmdp->_func(cmdp->argc, cmdp->argv);
}

int do_printenv(int argc, char **argv)
{
    char *env = getenv(argv[1]);
    if (argc != 2) {
        fprintf(stdout, "argc incorrect\n");
        fprintf(stdout, "printenv usage: printenv VAR\n");
        return -1;
    }
    if (env) {
        fprintf(stdout, "%s\n", env);
    }
    return 0;
}

int do_setenv(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stdout, "argc incorrect\n");
        fprintf(stdout, "setenv usage: setenv VAR AS_SOMETHING\n");
        return -1;
    }
    return setenv(argv[1], argv[2], 1);
}

int do_exit(int argc, char **argv)
{

#ifdef CONFIG_SERVER2
    Self.exit = 1;
    return 0;
#endif /* CONFIG_SERVER2 */

    exit(0);
}

int do_source(int argc, char **argv)
{
    return 0;
}
