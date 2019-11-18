#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

#include "net.h"

#ifdef CONFIG_SERVER3
#include "command.h"
#include "debug.h"

#include <string.h>
#include <signal.h>

/* fifo, open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>

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
#define BUFSIZE 256
#define PIPE_EXIST "*** Error: the pipe #%d->#%d already exists. ***"
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

int upipe_set_writeend(int dstid, int force)
{
    int fd;
    int rc;
    char path[BUFSIZE];
    
    rc = usrchk(dstid);
    if (rc) {
        return -1;
    }

    initialize_fifo(dstid, path);
    force_upipe(dstid, path, force);
    
    fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    write(fd, "ok!", 4);
    
    upipe_out[dstid] = fd;
    return fd;
}

#define PIPENOTFOUND "*** Error: the pipe #%d->#%d does not exist yet. ***"
int upipe_get_readend(int srcid)
{
    int rc;
    rc = usrchk(srcid);
    if (rc) {
        return -1;
    }
    
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
    
    fd = upipe_in[srcid];
    if (fd == -1) {
        dprintf(2, "bug in upipe_release\n");
    }
    close(fd);
    upipe_in[srcid] = -1;
    
    char path[BUFSIZE];
    snprintf(path, BUFSIZE, "%s/#%d_#%d", FIFO_PREFIX, srcid, selfid);
    unlink(path);
    return 0;
}
#endif /* CONFIG_SERVER3 */