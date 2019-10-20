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
    return 0;
}

int fd_inc(int fd)
{
    FDCHK(fd);
    if (FDtable[fd].ref == 0) {
        printf("Error, fd=%d does not exist\n", fd);
        exit(-1);
    }
    
    FDtable[fd].ref++;
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
        dprintf(0, "closing file descriptor %d", fd);
        close(fd);
    }
    return 0;
}