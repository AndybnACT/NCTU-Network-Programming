#include "_config.h"
#include <unistd.h>

int npclient_who(int argc, char *argv[]);
int npclient_name(int argc, char *argv[]);
int npclient_tell(int argc, char *argv[]);
int npclient_yell(int argc, char *argv[]);


#ifdef CONFIG_SERVER1
#define MAXUSR 1

#endif /* CONFIG_SERVER1 */


#ifdef CONFIG_SERVER3

#define MAXUSR 30
int npserver_init(void);
int npserver_reap_client(pid_t pid);
int npclient_init(int fd, char *inimsg);

#define MSG_SET       0x1
#define MSG_SIGNALED  0x2
#define MSG_COMPLETED 0x3

#define MSG_TYPE_TELL 0x1
#define MSG_TYPE_YELL 0x2
#define MSG_TYPE_SYS  0x3

struct message{
    int volatile dst_id;
    int volatile stat;
    int volatile type;
    char volatile message[1024];
};

#define RESET_USR(x){             \
    (x)->pid = -1;                \
    (x)->stat = 0;                \
    (x)->name[0] = '\0';          \
    (x)->signaled = 0;            \
    (x)->msg.dst_id = -2;         \
    (x)->msg.message[0] = '\0';   \
}

#define USTAT_FREE 0x0
#define USTAT_USED 0x1
#define USTAT_DEAD 0x2

struct usr_struct {
    pid_t pid;
    int id;
    int stat;
    char name[25];
    char netname[30];
    int signaled;
    struct message msg;
};

#define FOR_EACH_USR(i) for (; i < MAXUSR; i++) 
extern int selfid;
extern struct usr_struct *UsrLst;
#define Self (UsrLst[selfid])

struct mt_unsafe_mem_obj{
    void *base;
    void *top;
    void *limit;
};

#define PERUSRSIZE sizeof(struct usr_struct)
#define USRSIZE PERUSRSIZE*MAXUSR

#define ROUNDUP(x) (((x) + 0xFFF) & (~0xFFF))

#endif /* CONFIG_SERVER3 */
