#define _GNU_SOURCE // for var environ
#include "npshell.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "debug.h"
#include "command.h"


#define NP_ROOT "working_dir"

struct Command *Cmd_Head;

int npshell(int argc, char const *argv[])
{
    char *cmdbuf = NULL;
    size_t bufsize = 0;
    struct Command *cmd_cur;
    struct sigaction sigdesc;
    int ret;
    sigset_t blk_chld, orig;
    
    dprintf(0, "initializing npshell, pid=%d\n", getpid());
    
    Cmd_Head = zallocCmd();
    cmd_cur = Cmd_Head;
    sigdesc.sa_sigaction = npshell_sigchld_hdlr;
    sigdesc.sa_flags = SA_SIGINFO|SA_NOCLDSTOP;
    
    dprintf(0, "npshell: registering chld signal handler\n");
    ret = sigaction(SIGCHLD, &sigdesc, NULL);
    if (ret) {
        perror("sigaction");
        exit(-1);
    }
    
    ret = setenv("PATH", "bin:.", 1);
    if (ret) {
        perror("setenv");
        return errno;
    }
    
    Cmd_Head->stat = STAT_READY;
    Cmd_Head->next = NULL;
    
    sigemptyset(&blk_chld);
    sigaddset(&blk_chld, SIGCHLD);

    while (1) {
        sigprocmask(SIG_BLOCK, &blk_chld, &orig);
        printf("%% ");
retry:
        errno = 0;
        ret = getline(&cmdbuf, &bufsize, stdin);
        if (ret == -1) {
            if (errno == EINTR) {
                // dprintf(0, "interrupted syscall\n");
                goto retry;
            }
            perror("getline");
            free(cmdbuf);
            return errno;
        }
        sigprocmask(SIG_SETMASK, &orig, NULL);
        
        dprintf(0, "===>getline=%d:  %s\n", ret, cmdbuf);
        
        cmd_cur = parse2Cmd(cmdbuf, bufsize, Cmd_Head);
        
        dprintf(0, "-------------------------------\n")
        for (struct Command *p = Cmd_Head; p; p = p->next){
            dprintf(0, "......\n");
            dprintCmd(0, p);
        }
        dprintf(0, "-------------------------------\n")
        
        
        ret = execCmd(cmd_cur);
        
        // due to the reentrancy of signal handler, all set (STAT_SET) and 
        // fork'ed commands after syncCmd should not remain active (STAT_EXEC).
        // ==> This currently makes npshell do not support background command
        // ><><><><><><><><><><><><><>
        // We may touch only pid(r), stat(w), exit_code(w) of a executed Cmd in
        // the handler and we have properly block the signal when using those 
        // variables in 'exec.c'. Thus, it is possible to leave processes remain
        // STAT_EXEC if it does not need tty output
        Cmd_Head = syncCmd(Cmd_Head);
    }

    return 0;
}