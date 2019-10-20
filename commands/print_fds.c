#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    char cmd[10000] = "\0";
    pid_t pid = getpid();
    snprintf(cmd, 1000, "ls -l /proc/%d/fd/ > %d.out", pid, pid);
    for (size_t i = 0; i < argc; i++)
        printf( "[%d], argv[%ld] = %s\n",pid, i, argv[i]);
    system(cmd);
    return 0;
}