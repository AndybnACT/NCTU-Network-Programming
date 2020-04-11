#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

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

#ifdef CONFIG_SERVER1
struct dummy_self {
    struct env_struct curenv;
} Self;
#endif /* CONFIG_SERVER1 */

int get_envid(struct env_struct *envp, char *name)
{
    for (size_t i = 0; i < envp->top; i++) {
        if (strcmp(name, envp->key[i]) == 0)
            return i;
    }
    return -1;
}

char* np_getenv(char *name)
{
    struct env_struct *envp = &Self.curenv;
    int id = get_envid(envp, name);
    if (id == -1) {
        return NULL;
    }
    return envp->value[id];
}

int env_alloc(struct env_struct *envp)
{
    int lim = envp->lim + 10;
    if (envp->lim == 0) {
        envp->key = (char**) malloc(10*sizeof(char*));
        envp->value = (char**) malloc(10*sizeof(char*));
    }else{
        envp->key = (char**) realloc(envp->key, lim*sizeof(char*));
        envp->value = (char**) realloc(envp->value, lim*sizeof(char*));
    }
    if (!envp->key || !envp->value) {
        perror("alloc");
        exit(-1);
    }
    envp->lim = lim;
    return 0;
}

int np_setenv(char *name, char *value, int _dummy)
{
    struct env_struct *envp = &Self.curenv;
    int id = get_envid(envp, name);
    if (id == -1) {
        id = envp->top;
        if (envp->top >= envp->lim)
            env_alloc(envp);
        envp->key[id] = strdup(name);
        envp->value[id] = strdup(value);
        envp->top++;
    }else{
        free(envp->value[id]);
        envp->value[id] = strdup(value);
    }
    return 0;
}

int env_init()
{
    struct env_struct *envp = &Self.curenv;
    
    envp->lim = 0;
    envp->top = 0;
    // leak (alloc'ed but not free'ed) when reusing usr
    env_alloc(envp);
    np_setenv("PATH", "bin:.", 1);
    return 0;
}

static inline char * _copy_path(void)
{
    char *path;
    path = np_getenv("PATH");
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
    char *env = np_getenv(argv[1]);
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
    return np_setenv(argv[1], argv[2], 1);
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
