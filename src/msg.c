#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

#include "net.h"
#include "command.h"
#include "debug.h"
#include <string.h>

#if defined(CONFIG_SERVER2) || defined(CONFIG_SERVER3)

static inline int handle_msg(struct message *msg, int sender, int fd)
{
    int len = strlen((char*) msg->message);
    int namelen = strlen(UsrLst[sender].name);

#ifdef CONFIG_SERVER3
    if ((msg->dst_id == selfid) && msg->stat == MSG_SIGNALED) {
#endif /* CONFIG_SERVER3 */
        switch (msg->type) {
            case MSG_TYPE_SYS:
                write(fd, (char*) msg->message, len < 1024 ? len : 1024);
                break;
            case MSG_TYPE_TELL:
                write(fd, "*** ", 4);
                write(fd, UsrLst[sender].name, namelen < 25 ? namelen : 25 );
                write(fd, " told you ***: ", 15);
                write(fd, (char*) msg->message, len < 1024 ? len : 1024);
                write(fd, "\n", 2);
                break;
            case MSG_TYPE_YELL:
                write(fd, "*** ", 4);
                write(fd, UsrLst[sender].name, namelen < 25 ? namelen : 25 );
                write(fd, " yelled ***: ", 13);
                write(fd, (char*) msg->message, len < 1024 ? len : 1024);
                write(fd, "\n", 2);
                break;
            default:
                write(fd, "Error in handle_msg\n", 21);
                break;
#ifdef CONFIG_SERVER3
        }
#endif /* CONFIG_SERVER3 */        
        
        msg->dst_id = -1;
        msg->stat = MSG_COMPLETED;
        return 0;
    }
    return -1;
}

#ifdef CONFIG_SERVER2
int msg_init(void)
{
    // no explicit initialization routine is needed
    return 0;
}

int msg_send(struct message *msg, int force)
{
    int dstid = msg->dst_id;
    int dstfd;

    if (dstid == -1) {
        dstid = 1;
        FOR_EACH_USR(dstid){
            dstfd = UsrLst[dstid].connfd;
            handle_msg(msg, selfid, dstfd);
        }
    }else{
        dstfd = UsrLst[dstid].connfd;
        handle_msg(msg, selfid, dstfd);
    }
    return 0;
}
#endif /* CONFIG_SERVER2 */

#ifdef CONFIG_SERVER3
#include <signal.h>

void msg_hdlr(int sig, siginfo_t *info, void *ucontext)
{
    pid_t srcpid = info->si_pid;
    struct message *msg;
    int sender_id = 0;
    // first we print the message from sender
    
    FOR_EACH_USR(sender_id){
        if (UsrLst[sender_id].pid == srcpid) {
            msg = &UsrLst[sender_id].msg;
            handle_msg(msg, sender_id, 1);
            goto success;
        }
    }
    dprintf(0, "bug! cannot find message by pid=%d in UsrLst\n", srcpid);
    exit(-1);
success:
    return;
}

int msg_init(void)
{
    int rc;
    struct sigaction sigdesc;
    memset(&sigdesc, 0, sizeof(struct sigaction));

    sigdesc.sa_sigaction = msg_hdlr;
    sigdesc.sa_flags = SA_SIGINFO|SA_RESTART;
    // SA_RESTART is a must or getline may disfunction

    dprintf(0, "registering usr1 signal handler\n");
    rc = sigaction(SIGUSR1, &sigdesc, NULL);
    if (rc) {
        perror("sigaction");
        exit(-1);
    }
    
    return 0;
}

int flush_msg(struct message *msg, int flush)
{
    pid_t recv_pid = UsrLst[msg->dst_id].pid;
    int rc;
    msg->stat = MSG_SIGNALED;
    do {
        rc = kill(recv_pid, SIGUSR1);
        if (rc == -1) {
            perror("kill");
            return -1;
        }
    } while (msg->stat == MSG_SIGNALED && flush);
    // must block here so that we can use the same data structure for boardcasting
    return 0;
}

int msg_send(struct message *msg, int force)
{
    int id = msg->dst_id;
    if (id == -1) { // boardcast msg
        id = 1;
        FOR_EACH_USR (id) {
            if (id == selfid) {
                if (msg->type == MSG_TYPE_YELL) {
                    printf("*** %s yelled ***: %s\n",Self.name, msg->message);
                }else{
                    printf("%s", msg->message);
                }
            }else if (UsrLst[id].stat == USTAT_USED){
                dprintf(1, "%d sending msg to %d\n", selfid, id);
                msg->dst_id = id;
                flush_msg(msg, force);
            }
        }
        return 0;
    }
    
    flush_msg(msg, force);    
    return 0;
}
#endif /* CONFIG_SERVER3 */
#endif /* CONFIG_SERVER3 || CONFIG_SERVER2 */
