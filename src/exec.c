#define _GNU_SOURCE 
#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include "command.h"
#include "debug.h"
#include "net.h"
#include "upipe.h"


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
extern struct Command *Cmd_Head;
void npshell_sigchld_hdlr(int sig, siginfo_t *info, void *ucontext)
{
    int status = 0;
    pid_t pid; // = info->si_pid;  !! pending `SIGCHLD`s can be coalesced
    struct Command *signaled_cmd;
    
    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            break;
        }
        dprintf(0, "npshell_sigchld_hdlr: unregistering pid %d \n", pid);
        
        FINDCMD_BY_PID(signaled_cmd, pid, Cmd_Head);
        if (!signaled_cmd) {
            exit(-1);
        }
        
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            signaled_cmd->stat = STAT_KILL;
            signaled_cmd->exit_code = WIFEXITED(status) ? 
                                     WEXITSTATUS(status):WTERMSIG(status);
#ifdef CONFIG_SERVER3
            upipe_release(signaled_cmd->upipe[0]);
#endif /* CONFIG_SERVER3 */
            signaled_cmd->stat = STAT_FINI;
        }else{
            dprintf(0, "npshell_sigchld_hdlr: Error !!!\n");
            signaled_cmd->stat = STAT_KILL;
        }
    }
    return;
}

int child_dupfd(int old, int new)
{ // only used in child
    int rc = 0;
    if (old != -1 && new != -1) {
        close(new);
        rc = dup2(old, new);
        if (rc == -1){
            perror("dup");
            exit(-1);
        }
        // should not close old since stderr may use the same 'old' fd
        // O_CLOEXEC will close the fds for us
    }
    // fail silently for invalid arguments
    return rc;
}


int _create_pipe(int *pipefds)
{
    int ret;
retry:
    ret = pipe2(pipefds, O_CLOEXEC);
    if (ret == -1) {
        perror("pipe ");
        // Error handling
        switch (errno) {
            case EMFILE:
            case ENFILE:
                printf("number of open fd has been reach\n");
                goto retry;
        }
        return -1;
    }
    dprintf(2, "\tcreate pipe, fd = %d, %d\n", pipefds[0], pipefds[1]);
    return 0;
}
//                                                      write-side    read-side
//                                                         _w -----------> _r
int fill_pipe_fd(struct Command *source, struct Command *dest, int _w, int _r)
{
    int pipes[2];
    int rc;
    int *src_fds_ptr = source->fds;
    int *dst_fds_ptr = dest->fds;
    
    if (dst_fds_ptr[_r] == -1) {
        rc = _create_pipe(pipes);
        if (rc) {
            printf("Error creating pipe\n");
            exit(-1);
        }
        src_fds_ptr[_w] = pipes[1];
        dst_fds_ptr[_r] = pipes[0];
        dest->pipes[0] = pipes[0];
        dest->pipes[1] = pipes[1]; 
    }else{
        /* pipe already exists. someone has allocated a pipe to the dest */
        if (dest->pipes[1] == -1) {
            printf("BUG, in-pipe allocated but cannot find out-pipe fd\n");
            exit(-1);
        }
        src_fds_ptr[_w] = dest->pipes[1];
    }
    return 0;
}

#ifdef CONFIG_SERVER3
int percmd_upipe(struct Command *percmd)
{
    int fd;
    if (percmd->upipe[0] != -1) {
        if (percmd->fds[0] != -1) {
            printf("Error, input of upipe conflicts with preallocated pipes\n");
            exit(-1);
        }
        fd = upipe_get_readend(percmd->upipe[0]);
        if (fd < 0) {
            percmd->upipe[0] = -1;
            fd = -1;
        }
        percmd->fds[0] = fd;
    }
    if (percmd->upipe[1] != -1) {
        if (percmd->fds[1] != -1) {
            printf("Error, output of upipe conflicts with preallocated pipes\n");
            exit(-1);
        }
        fd = upipe_set_writeend(percmd->upipe[1], 1);
        if (fd < 0) {
            percmd->upipe[0] = -1;
            fd = -1;
        }
        percmd->fds[1] = fd;
    }
    return 0;
}
#endif /* CONFIG_SERVER3 */

int percmd_pipes(struct Command *percmd)
{
    int ret = -1;

    if (percmd->cmd_out_pipe) {
        fill_pipe_fd(percmd, percmd->cmd_out_pipe, 1, 0);
    }
    
    if (percmd->cmd_err_pipe) {
        fill_pipe_fd(percmd, percmd->cmd_err_pipe, 2, 0);
    }
    
    if (percmd->cmd_first_in_pipe && percmd->fds[0] == -1) {
        printf("BUG, in pipe exist but pipefd = -1\n");
        exit(-1);
    }
    
    return ret; // need error handling
}

int percmd_file(struct Command *percmd)
{
    int fd;
    char *path = percmd->file_out_pipe;
    if (!path)
        return 0;
    
    fd = open(path, O_RDWR|O_CLOEXEC|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) {
        perror("open ");
        exit(-1);
    }
    
    if (percmd->fds[1] != -1) {
        printf("BUG, output fd already exists\n");
        exit(-1);
    }
    
    percmd->fds[1] = fd;
    
    return 0;
}

int child_source_exec(struct Command *child)
{
    int argc = child->argc;
    int fd;
    if (argc != 2) {
        printf("source usage: source file\n");
        exit(-1);
    }
    fd = open(child->argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }
    if (child->fds[0] != -1) {
        printf("source BUG, stdin already allocated\n");
    }
    child->fds[0] = fd;
    return 0;
}

int percmd_exec(struct Command *percmd)
{
    pid_t child;
    int ret;
    sigset_t blk_chld, orig;
    int fderr;
    int fd;
    
    if (percmd->exec[0] != '.' && percmd->exec[0] != '/') {
        // lookup in PATH and builtin cmd
        ret = command_lookup(percmd);
        if (ret < 0) {
            if (ret == -CMD_BUILTIN) {
                // last chance to do piping over here 
                ret = _builtin_cmd_exec(percmd);
                percmd->stat = STAT_FINI;
                percmd->exit_code = ret;
            }else if (ret == -CMD_UNKNOWN) {
                fderr = percmd->fds[2] == -1 ? 2:percmd->fds[2];
                
                percmd->stat = STAT_FINI;
                percmd->exit_code = -1;
                // write to stderr of this cmd !!
                write(fderr, "Unknown command: [", 18);
                write(fderr, percmd->exec, strlen(percmd->exec));
                write(fderr, "].\n", 3);
                ret = -1;
            }
            goto safe_close;
        }
    }

    sigemptyset(&blk_chld);
    sigaddset(&blk_chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blk_chld, &orig);
    if (sigismember(&orig, SIGCHLD)) { // not reach
        printf("BUG, SIGCHLD is member of original signal mask\n");
        sigdelset(&orig, SIGCHLD);
    }

retry:
    percmd->stat = STAT_EXEC;
    child = fork();
    if (child == -1) {
        // perror("fork ");
        sigsuspend(&orig);
        goto retry;
    }else if (child == 0){ // in child
        sigprocmask(SIG_SETMASK, &orig, NULL);
        dprintf(1, "hello from child %d\n", getpid());
        if (percmd->source) {
            child_source_exec(percmd);
        }
        child_dupfd(percmd->fds[0], 0);
        child_dupfd(percmd->fds[1], 1);
        child_dupfd(percmd->fds[2], 2);
        // ret = execve("./bin/print_fds", percmd->argv, Envp); // for dbgrun or testrun
        ret = execve(percmd->exec, percmd->argv, environ);
        if (ret) {
            perror("execve ");
        }
        exit(-1);
    }
    
    // in parent
    dprintf(1, "%d forked\n", child);
    percmd->pid  = child;
    sigprocmask(SIG_SETMASK, &orig, NULL);
    
    dprintf(1, "cmd struct of %d:\n", child);
    dprintCmd(1, percmd);
    dprintf(1, "-------------------\n");

safe_close:    
    SAFE_CLOSEFD(percmd->fds[0]);
    SAFE_CLOSEFD(percmd->pipes[0]);
    SAFE_CLOSEFD(percmd->pipes[1]);
    if (percmd->upipe[1] != -1) {
        fd = upipe_get_writeend(percmd->upipe[1]);
        close(fd);
    }
    if (percmd->file_out_pipe)
        SAFE_CLOSEFD(percmd->fds[1]);
    
    return ret;
}

int execCmd(struct Command *Cmdhead)
{
    int cnt = 0;
    struct Command *Cmd;
    
    Cmd = Cmdhead;
    while (Cmd->stat != STAT_READY) {
        if (Cmd->stat == STAT_SET) {
#ifdef CONFIG_SERVER3
            percmd_upipe(Cmd);
#endif /* CONFIG_SERVER3 */
            percmd_pipes(Cmd);
            percmd_file(Cmd);
            percmd_exec(Cmd); // do fork-exec routines
            cnt++;
            Cmd = Cmd->next;
        }else{
            printf("BUG, execCmd reaches:\n");
            dprintCmd(0, Cmd);
            printf("======================\n");
        }
    }
    return cnt;
}

/*  We need a signal handler to handle cases where fork fails.
 *  However, we dont't use waitpid related functions here in order to prevent
 *  mixing same code used to unregister Command struct into two seperated places
 *  (signal handler & code after waitpid).
 *  We should not use command count returned from execCmd as breaking condition 
 *  of the while loop since the active command count may not remain same due
 *  to the signal handler 
 */
static inline int _need_ttyo(struct Command *p)
{
    if (p->stat != STAT_EXEC)
        return -1;
    if (p->fds[1] == -1) {
        return 1;
    }else{
        return 0;
    }
}
static inline int _block(struct Command *h)
{
    int blk;
    for (struct Command *p = h; p; p=p->next) {
        blk = 0;
        if (p->stat == STAT_EXEC) {
            if (_need_ttyo(p) == 1 || p->file_out_pipe) {
                // file output should be sequential!!
                blk = 1;
                break;
            }
        }
    }
    return blk;
}

struct Command * syncCmd(struct Command *head)
{
    sigset_t orig, wait_chld;
    int block = 1;
    
    sigemptyset(&wait_chld);
    sigaddset(&wait_chld, SIGCHLD);
    
    sigprocmask(SIG_BLOCK, &wait_chld, &orig);
    block = _block(head);
    while (block) {
        dprintf(1, "waiting for child msk=%d\n", sigismember(&orig, SIGCHLD));
        sigsuspend(&orig); // sigsuspend return after signal is handled
        block = _block(head);
    }
    
    if (sigismember(&orig, SIGCHLD)) { // not reach
        printf("BUG, SIGCHLD is member of original signal mask\n");
        sigdelset(&orig, SIGCHLD);
    }
    
    sigprocmask(SIG_SETMASK, &orig, NULL);
    
    return head;
}

