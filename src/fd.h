#define FDMAX 4096
struct file_desc{
    int ref;
};
extern struct file_desc FDtable[FDMAX];

int fd_start(void);
int fd_init(int fd);
int fd_inc(int fd);
int fd_dec(int fd);


