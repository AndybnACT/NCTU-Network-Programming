#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "fd.h"
#include "debug.h"
#include "command.h"

struct file_desc FDtable[FDMAX] = {0, };

#define FDCHK(fd){                                      \
    if (fd < 0 || fd >= FDMAX) {                        \
        printf("ERROR!! fd out of range (%d)\n", fd);   \
        exit(-1);                                       \
    }                                                   \
}

int fd_start()
{
    memset(FDtable, 0, FDMAX*sizeof(struct file_desc));
    return 0;
}

int fd_init(int fd)
{
    FDCHK(fd);
    if (FDtable[fd].ref != 0) {
        printf("Error, fd=%d exists, ref count = %d\n",fd, FDtable[fd].ref);
        exit(-1);
    }
    FDtable[fd].ref = 1;
    FDtable[fd].r_end = -1;
    FDtable[fd].w_end = -1;
    return 0;
}


int fd_pipe_init(int fdr, int fdw)
{
    fd_init(fdr);
    fd_init(fdw);
    FDtable[fdr].w_end = fdw; 
    FDtable[fdw].r_end = fdr;
    FDtable[fdr].ref = 2;
    FDtable[fdw].ref = 2;
    return 0;
}

int fd_inc(int fd)
{
    FDCHK(fd);
    if (FDtable[fd].ref == 0) {
        printf("Error, fd=%d does not exist\n", fd);
        exit(-1);
    }
    dprintf(0, "increasing ref count of %d (=> %d)", fd, FDtable[fd].ref + 1);
    FDtable[fd].ref++;
    return 0;
}

int fd_pipe_inc(int fd)
{
    int rpipe = FDtable[fd].r_end;
    int wpipe = FDtable[fd].w_end;
    fd_inc(fd);
    // if fd is not created by pipe, then the following calls fail silently
    if (rpipe != -1) 
        fd_inc(rpipe);
    else if (wpipe != -1)
        fd_inc(wpipe);
    return 0;
}

int fd_dec(int fd)
{
    FDCHK(fd);
    if (FDtable[fd].ref == 0) {
        printf("Error, fd=%d does not exist\n", fd);
        exit(-1);
    }
    
    FDtable[fd].ref--;
    if (FDtable[fd].ref == 0) {
        dprintf(0, "closing file descriptor %d\n", fd);
        close(fd);
    }
    return 0;
}

int fd_pipe_dec(int fd)
{
    int rpipe = FDtable[fd].r_end;
    int wpipe = FDtable[fd].w_end;
    fd_dec(fd);
    // if fd is not created by pipe, then the following calls fail silently
    if (rpipe != -1) 
        fd_dec(rpipe);
    else if (wpipe != -1)
        fd_dec(wpipe);
    return 0;
}
