#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif
#define _GNU_SOURCE 

#include "net.h"
#include "command.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFSIZE 256
#define PIPE_EXIST "*** Error: the pipe #%d->#%d already exists. ***\n"

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
        fprintf(stdout, PIPE_EXIST, selfid, dstid);
        dprintf(2, "UsrLst[%d].upipe_in[%d] = %d\n", 
                    dstid, selfid, UsrLst[dstid].upipe_in[selfid]);
        return -1;
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


int upipe_in[MAXUSR] = {-1, };
int upipe_out[MAXUSR] = {-1, };

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
    dprintf(0, "bug! cannot find message by pid=%d in UsrLst\n", srcpid);
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
    upipe_in[sender_id] = fd;
    return;
}

int upipe_init()
{
    int rc;
    struct sigaction sigdesc;
    
    dprintf(1, "initializing upipes\n")
    memset(upipe_in, -1, MAXUSR*sizeof(int));
    memset(upipe_out, -1, MAXUSR*sizeof(int));
    
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
        fprintf(stdout, PIPE_EXIST, selfid, dstid);
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
    int rc;
#ifdef CONFIG_SERVER3
    char path[BUFSIZE];
#endif /* CONFIG_SERVER3 */
    
    rc = usrchk(dstid);
    if (rc) {
        return -1;
    }

#ifdef CONFIG_SERVER2
    fd = upipe_set_pipes(dstid);
#endif /* CONFIG_SERVER2 */

#ifdef CONFIG_SERVER3
    initialize_fifo(dstid, path);
    force_upipe(dstid, path, force);
    
    fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    write(fd, "ok!", 4); 
        
    upipe_out[dstid] = fd;
#endif /* CONFIG_SERVER3 */

    return fd;
}

#define PIPENOTFOUND "*** Error: the pipe #%d->#%d does not exist yet. ***\n"
int upipe_get_readend(int srcid)
{
    int rc;
    rc = usrchk(srcid);
    if (rc) {
        return -1;
    }
    
#ifdef CONFIG_SERVER2
    int *upipe_in = Self.upipe_in;
#endif /* CONFIG_SERVER2 */    
    
    if (upipe_in[srcid] == -1) {
        fprintf(stdout, PIPENOTFOUND, srcid, selfid);
        return -1;
    }
    
    return upipe_in[srcid];
}

int upipe_get_writeend(int dstid)
{
    int rc;
    rc = usrchk(dstid);
    if (rc) {
        return -1;
    }
    
#ifdef CONFIG_SERVER2
    int *upipe_out = Self.upipe_out;
#endif /* CONFIG_SERVER2 */    
    
    if (upipe_out[dstid] == -1) {
        fprintf(stdout, PIPENOTFOUND, dstid, selfid);
        return -1;
    }
    
    return upipe_out[dstid];
}


int upipe_release(int srcid)
{
    int fd;
    if (srcid == -1)
        return 0;
    
#ifdef CONFIG_SERVER2
    int *upipe_in = Self.upipe_in;
#endif /* CONFIG_SERVER2 */

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
#endif /* CONFIG_SERVER2 || CONFIG_SERVER3 */
