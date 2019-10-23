#define _GNU_SOURCE 
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include "command.h"
#include "debug.h"

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
    
    fd = open(path, O_RDWR|O_CLOEXEC|O_CREAT, 0666);
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

int percmd_exec(struct Command *percmd)
{
    pid_t child;
    int ret;
    sigset_t blk_chld, orig;
    int fderr;
    
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
        child_dupfd(percmd->fds[0], 0);
        child_dupfd(percmd->fds[1], 1);
        child_dupfd(percmd->fds[2], 2);
        // ret = execve("./bin/print_fds", percmd->argv, Envp); // for dbgrun or testrun
        ret = execve(percmd->exec, percmd->argv, Envp);
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

