#define FDMAX 4096
struct file_desc{
    int r_end;
    int w_end;
    int ref;
    int maxref;
};
extern struct file_desc FDtable[FDMAX];

int fd_start(void);
int fd_pipe_init(int fdr, int fdw);
int fd_pipe_inc(int fd);
int fd_pipe_dec(int fd);
int fd_init(int fd);
// int fd_dec(int fd);


