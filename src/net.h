#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

#include <unistd.h>

int npclient_who(int argc, char *argv[]);
int npclient_name(int argc, char *argv[]);
int npclient_tell(int argc, char *argv[]);
int npclient_yell(int argc, char *argv[]);


#ifdef CONFIG_SERVER1
#define MAXUSR 1

#endif /* CONFIG_SERVER1 */


#if defined(CONFIG_SERVER3) || defined(CONFIG_SERVER2)
#ifdef CONFIG_SERVER2
#include "command.h"
#endif
#define MAXUSR 30

#include <stdio.h>
int npserver_init(void);
int npclient_init(int fd, char *inimsg);

#ifdef CONFIG_SERVER3
int npserver_reap_client(pid_t pid);
#endif /* CONFIG_SERVER3 */
#ifdef CONFIG_SERVER2
int npserver_reap_client(int id);
#endif /* CONFIG_SERVER2 */

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

struct user_pipe{
    int volatile signaled;
    char volatile path[512];
};

#define RESET_USR(x){             \
    (x)->pid = -1;                \
    (x)->stat = 0;                \
    (x)->name[0] = '\0';          \
    (x)->upipe.signaled = 0;      \
    (x)->upipe.path[0] = '\0';    \
    (x)->msg.dst_id = -2;         \
    (x)->msg.message[0] = '\0';   \
}

#ifdef CONFIG_SERVER2
#undef RESET_USR
#define RESET_USR(x){             \
    (x)->pid = -1;                \
    (x)->stat = 0;                \
    (x)->name[0] = '\0';          \
    (x)->upipe.signaled = 0;      \
    (x)->upipe.path[0] = '\0';    \
    (x)->msg.dst_id = -2;         \
    (x)->msg.message[0] = '\0';   \
    (x)->exit = 0;                \
    (x)->connfd = -1;             \
    (x)->in = NULL;               \
    (x)->out = NULL;              \
    (x)->err = NULL;              \
    RESETCMD(&((x)->cmdhead));    \
}
#endif /* CONFIG_SERVER2 */

#define USTAT_FREE 0x0
#define USTAT_USED 0x1
#define USTAT_DEAD 0x2

struct usr_struct {
    pid_t pid;
    int id;
    int stat;
    char name[25];
    char netname[30];
    struct user_pipe upipe;
    struct message msg;
#ifdef CONFIG_SERVER2
    int exit;
    int connfd;
    FILE *in;
    FILE *out;
    FILE *err;
    struct Command cmdhead;
    int upipe_in[MAXUSR];
    int upipe_out[MAXUSR];
#endif /* CONFIG_SERVER2 */
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

#define USRNOTFOUND(id) "*** Error: user #%d does not exist yet. ***\n", (id)
static inline int usrchk(int dstid)
{
    do {                                            
        if ((dstid) < MAXUSR && (dstid) > 0){       
            if (UsrLst[dstid].stat == USTAT_USED) { 
                break;                              
            }                                       
        }                                           
        fprintf(stdout, USRNOTFOUND(dstid));        
        return -1;                                  
    } while(0);
    return 0;
}

#endif /* CONFIG_SERVER3 || CONFIG_SERVER2 */
