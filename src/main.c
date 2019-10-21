#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "debug.h"
#include "command.h"

#include <sys/types.h>
#include <sys/wait.h>

#define NP_ROOT "working_dir"

char **Envp;
struct Command *Cmd_Head;

#define FINDCMD_BY_PID(ptr, pid, head){                         \
    ptr = head;                                                 \
    while (ptr->pid != pid) {                                   \
        ptr = ptr->next;                                        \
        if (ptr->next == NULL) {                                \
            dprintf(0, "error finding Cmd with pid = %d", pid); \
            ptr = NULL;                                         \
            break;                                              \
        }                                                       \
    }                                                           \
}

// read:
//      SA_NOCLDSTOP
//      SA_NOCLDWAIT
void sigchld_hdlr(int sig, siginfo_t *info, void *ucontext)
{
    int status = 0;
    pid_t pid; // = info->si_pid;  !! pending `SIGCHLD`s can be coalesced
    struct Command *signaled_cmd;
    
    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            break;
        }
        dprintf(0, "sigchld_hdlr: unregistering pid %d \n", pid);
        
        FINDCMD_BY_PID(signaled_cmd, pid, Cmd_Head);
        if (!signaled_cmd) {
            exit(-1);
        }
        
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            signaled_cmd->stat = STAT_KILL;
            signaled_cmd->exit_code = WIFEXITED(status) ? 
                                     WEXITSTATUS(status):WTERMSIG(status);
            signaled_cmd->stat = STAT_FINI;
        }else{
            dprintf(0, "sigchld_hdlr: Error !!!\n");
            signaled_cmd->stat = STAT_KILL;
        }
    }
    return;
}

int main(int argc, char const *argv[], char *envp[])
{
    char *cmdbuf = NULL;
    size_t bufsize = 0;
    struct Command *cmd_cur;
    struct sigaction sigdesc;
    int ret;
    
    Cmd_Head = zallocCmd();
    cmd_cur = Cmd_Head;
    Envp = envp;
    sigdesc.sa_sigaction = sigchld_hdlr;
    sigdesc.sa_flags = SA_SIGINFO|SA_NOCLDSTOP;
    
    dprintf(0, "registering signal handler\n");
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
    
    while (1) {
        printf("%% ");
        
        errno = 0;
        ret = getline(&cmdbuf, &bufsize, stdin);
        if (ret == -1) {
            if (errno)
                perror("getline");
            free(cmdbuf);
            return errno;
        }
        
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
        Cmd_Head = syncCmd(Cmd_Head);
    }

    return 0;
}