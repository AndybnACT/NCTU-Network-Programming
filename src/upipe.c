#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif
#define _GNU_SOURCE 

#include "upipe.h"
#include "net.h"
#include "command.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFSIZE 256

#ifdef CONFIG_SERVER2

int upipe_init()
{
    memset(Self.upipe_in, -1, sizeof(int)*MAXUSR);
    memset(Self.upipe_out, -1, sizeof(int)*MAXUSR);
    
    return 0;
}

int upipe_set_pipes(int dstid)
{
    int rc;
    int fds[2];
    
    if (UsrLst[dstid].upipe_in[selfid] != -1) {
        // fprintf(stdout, PIPE_EXIST, selfid, dstid);
        dprintf(2, "Pipe exist! UsrLst[%d].upipe_in[%d] = %d\n", 
                    dstid, selfid, UsrLst[dstid].upipe_in[selfid]);
        return ERR_PIPE_EXIST;
    }
    
retry:
    rc = pipe2(fds, O_CLOEXEC);
    if (rc == -1) {
        perror("upipe pipe");
        // Error handling
        switch (errno) {
            case EMFILE:
            case ENFILE:
                printf("number of open fd has been reach\n");
                goto retry;
        }
        return -1;
    }
    Self.upipe_out[dstid] = fds[1];
    UsrLst[dstid].upipe_in[selfid] = fds[0];
    return fds[1];
}

#endif /* CONFIG_SERVER2 */

#ifdef CONFIG_SERVER3
#include <signal.h>

/* fifo, open */
#include <sys/types.h>

#define FIFO_PREFIX "./user_pipe/"

void upipe_hdlr(int sig, siginfo_t *info, void *ucontext)
{
    int fd;
    pid_t srcpid = info->si_pid;
    int sender_id = 1;
    struct user_pipe *upipe_p = &Self.upipe;
    char readbuf[10];
    
    dprintf(2, "hello from upipe_hdlr\n");
    if (!upipe_p->signaled) {
        return;
    }
    FOR_EACH_USR(sender_id){
        if (UsrLst[sender_id].pid == srcpid) {        
            goto success;
        }
    }
    dprintf(0, "upipe: bug! cannot find message by pid=%d in UsrLst\n", srcpid);
    exit(-1);
    
success:
    upipe_p->signaled = 0;
    fd = open((char*)upipe_p->path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }
    read(fd, readbuf, 4);
    dprintf(2, "===> %s\n", readbuf);
    Self.upipe_in[sender_id] = fd;
    return;
}

int upipe_init()
{
    int rc;
    struct sigaction sigdesc;
    
    dprintf(1, "initializing upipes\n")
    memset(Self.upipe_in, -1, MAXUSR*sizeof(int));
    memset(Self.upipe_out, -1, MAXUSR*sizeof(int));
    
    memset(&sigdesc, 0, sizeof(struct sigaction));

    sigdesc.sa_sigaction = upipe_hdlr;
    sigdesc.sa_flags = SA_SIGINFO|SA_RESTART;
    // SA_RESTART is a must or getline may disfunction

    dprintf(0, "registering usr2 signal handler for upipes\n");
    rc = sigaction(SIGUSR2, &sigdesc, NULL);
    if (rc) {
        perror("sigaction");
        exit(-1);
    }
    
    return 0;
}
int initialize_fifo(int dstid, char *path)
{
    int rc;
    
    snprintf(path, BUFSIZE, "%s/#%d_#%d", FIFO_PREFIX, selfid, dstid);
    dprintf(2, "testing if fifo %s exists\n", path);
    rc = access(path, F_OK);
    if (rc == 0) {
        dprintf(2, "fifo already exists, aborting\n");
        dprintf(2, PIPE_EXIST, selfid, dstid);
        return -1;
    }
    
    rc = mkfifo(path, 0666);
    if (rc == -1) {
        perror("mkfifo");
        exit(-1);
    }
    return 0;
}

int force_upipe(int dstid, char *pth, int force)
{
    int rc;
    struct usr_struct *dst = &UsrLst[dstid];
    int len = strlen(pth);
    len = len > BUFSIZE ? BUFSIZE : len;
    
    memcpy((char*)dst->upipe.path, pth, len);
    dst->upipe.signaled = 1;
    do {
        dprintf(2, "sending SIGUSR2 to %d\n", dst->pid);
        rc = kill(dst->pid, SIGUSR2);
        if (rc == -1) {
            perror("kill");
            return -1;
        }
    } while(dst->upipe.signaled && force);

    return 0;
}

#endif /* CONFIG_SERVER3 */


#if defined(CONFIG_SERVER2) || defined(CONFIG_SERVER3)
int upipe_set_writeend(int dstid, int force)
{
    int fd;
#ifdef CONFIG_SERVER3
    int rc;
    char path[BUFSIZE];
#endif /* CONFIG_SERVER3 */

#ifdef CONFIG_SERVER2
    fd = upipe_set_pipes(dstid);
#endif /* CONFIG_SERVER2 */

#ifdef CONFIG_SERVER3
    rc = initialize_fifo(dstid, path);
    if (rc == -1) {
        return ERR_PIPE_EXIST;
    }
    
    force_upipe(dstid, path, force);
    
    fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }
    write(fd, "ok!", 4); 
        
    Self.upipe_out[dstid] = fd;
#endif /* CONFIG_SERVER3 */

    return fd;
}

int upipe_get_readend(int srcid)
{
    int *upipe_in = Self.upipe_in;
    
    if (upipe_in[srcid] == -1) {
        dprintf(0, PIPENOTFOUND, srcid, selfid);
        return ERR_PIPENOTFOUND;
    }
    
    return upipe_in[srcid];
}

int upipe_get_writeend(int dstid)
{
    int *upipe_out = Self.upipe_out;
    
    if (upipe_out[dstid] == -1) {
        dprintf(0, PIPENOTFOUND, dstid, selfid);
        return ERR_PIPENOTFOUND;
    }
    
    return upipe_out[dstid];
}


int upipe_release(int srcid)
{
    int fd;
    if (srcid == -1)
        return 0;
    
    int *upipe_in = Self.upipe_in;

    fd = upipe_in[srcid];
    if (fd == -1) {
        dprintf(2, "bug in upipe_release\n");
    }
    close(fd);
    upipe_in[srcid] = -1;
    
#ifdef CONFIG_SERVER3
    char path[BUFSIZE];
    snprintf(path, BUFSIZE, "%s/#%d_#%d", FIFO_PREFIX, srcid, selfid);
    unlink(path);
#endif /* CONFIG_SERVER3 */
    return 0;
}

int upipe_release_all(int dstid)
{
    int fd;
    int *upipe_in = UsrLst[dstid].upipe_in;
    
    for (int srcid = 1; srcid < MAXUSR; srcid++) {
        fd = upipe_in[srcid];
        if (fd != -1){
            // must not to close anything sice this function is on performed by npserver
            // close(fd);
            upipe_in[srcid] = -1;
            
#ifdef CONFIG_SERVER3
            char path[BUFSIZE];
            snprintf(path, BUFSIZE, "%s/#%d_#%d", FIFO_PREFIX, srcid, dstid);
            unlink(path);
#endif /* CONFIG_SERVER3 */
        }
    }
    
    return 0;
}
#endif /* CONFIG_SERVER2 || CONFIG_SERVER3 */
