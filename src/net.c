#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

#include "net.h"

#ifdef CONFIG_SERVER1
int npclient_who(int argc, char *argv[]) {return -1;}
int npclient_name(int argc, char *argv[]) {return -1;}
int npclient_tell(int argc, char *argv[]) {return -1;}
int npclient_yell(int argc, char *argv[]) {return -1;}
#endif /* CONFIG_SERVER1 */

#if defined(CONFIG_SERVER3) || defined(CONFIG_SERVER2)
#include "command.h"
#include "msg.h"
#include "upipe.h"
#include "env.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h> /* ftruncate */
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h> /* For O_* constants */ 


int selfid;
struct usr_struct *UsrLst;
struct mt_unsafe_mem_obj shmem_obj;

#ifdef CONFIG_SERVER3
static int np_shmem_init(int shmfd, int prot)
{
    shmem_obj.base = mmap(NULL, ROUNDUP(USRSIZE), prot, MAP_SHARED, shmfd, 0);
    if (shmem_obj.base == MAP_FAILED)
        return -1;
    shmem_obj.top  = (void*)((char*)shmem_obj.base + USRSIZE);
    shmem_obj.limit= (void*)((char*)shmem_obj.base + ROUNDUP(USRSIZE));
    return 0;
}
#endif /* CONFIG_SERVER3 */

int npserver_init()
{
    int fd = -1;
    
#ifdef CONFIG_SERVER3
    int rc;
    fd = shm_open(SHM_NAME, O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(-1);
    }
    
    dprintf(1, "allocating shared memory, size=0x%lx\n", ROUNDUP(USRSIZE));
    rc = ftruncate(fd, ROUNDUP(USRSIZE));
    if (rc == -1) {
        perror("ftruncate");
        exit(-1);
    }
    
    rc = np_shmem_init(fd, PROT_READ|PROT_WRITE);
    if (rc == -1) {
        perror("server mmap");
        exit(-1);
    }
    UsrLst = (struct usr_struct *)shmem_obj.base;
#endif /* CONFIG_SERVER3 */

#ifdef CONFIG_SERVER2
    UsrLst = (struct usr_struct *) malloc(MAXUSR*sizeof(struct usr_struct));
#endif /* CONFIG_SERVER2 */

    for (int i = 0; i < MAXUSR; i++) {
        RESET_USR(&UsrLst[i]);
        UsrLst[i].id = i;
    }
    
    // id 0 is reserved for server
    UsrLst[0].stat = USTAT_USED;
    UsrLst[0].pid  = getpid();
    strcpy(UsrLst[0].name, "npserver");
    selfid = 0;
    
#ifdef CONFIG_SERVER2
    RESETCMD(&Self.cmdhead);
    Self.in = stdin;
    Self.out = stdout;
    Self.err = stderr;
#endif /* CONFIG_SERVER2 */
    
    dprintf(1, "npserver: shared memory mapped @ %p, current top = %p, limit = %p\n",
                shmem_obj.base, shmem_obj.top, shmem_obj.limit);
    
    return fd;
}

int npserver_cleanup()
{
#ifdef CONFIG_SERVER3
    int rc;
    rc = shm_unlink(SHM_NAME);
    if (rc == -1) {
        perror("shm_unlink");
    }
    // remove all user pipe
    
#endif /* CONFIG_SERVER3 */
    return 0;
}


#define LEAVE_MSG(n) "*** User '%s' left. ***\n", (n)

#ifdef CONFIG_SERVER3
int npserver_reap_client(pid_t pid)
{
    int killid;
    struct message *msg = &Self.msg;
    for (killid = 1; killid < MAXUSR; killid++) {
        if (UsrLst[killid].pid == pid) {
            
            UsrLst[killid].stat = USTAT_DEAD;
            snprintf((char *)msg->message, 1024,
                    LEAVE_MSG(UsrLst[killid].name));
            msg->dst_id = -1;
            msg->type = MSG_TYPE_SYS;
            msg_send(msg, 1);
            upipe_release_all(killid);
            
            RESET_USR(&UsrLst[killid]);
            return 0;
        }
    }
    return -1;
}
#endif /* CONFIG_SERVER3 */
#ifdef CONFIG_SERVER2
int npserver_reap_client(int id)
{
    struct message *msg = &Self.msg;
    
    close(UsrLst[id].connfd);
    fclose(UsrLst[id].in);
    fclose(UsrLst[id].out);
    fclose(UsrLst[id].err);
    
    snprintf((char *)msg->message, 1024,
            LEAVE_MSG(UsrLst[id].name));
    msg->dst_id = -1;
    msg->type = MSG_TYPE_SYS;
    msg_send(msg, 1);
    upipe_release_all(id);
    
    RESET_USR(&UsrLst[id]);
    return 0;
}
#endif /* CONFIG_SERVER2 */


int client_initialize_self()
{
    int id;
    for (id = 1; id < MAXUSR; id++) {
        if (UsrLst[id].pid == -1)
            break;
    }
    if (UsrLst[id].pid != -1) {
        dprintf(1, "Bug, number of active client exceed MAXUSR\n");
        exit(-1);
    }
    selfid = id;
    Self.pid = getpid();
    Self.stat = USTAT_USED;
    strcpy(Self.name, "(no name)");
    return id;
}

#define SEPERATOR "****************************************\n"
#define BANNERMSG "** Welcome to the information server. **\n"
#define PERUSRMSG(ipbuf) "*** User '(no name)' entered from %s. ***\n", (ipbuf)

extern int stream_forward(const int fd, FILE **stream_ptr, char *mode);
int npclient_init(int fd, char *ipmsg)
{
    int id;
    
    id = client_initialize_self();
    memcpy(Self.netname, ipmsg, strlen(ipmsg));
#ifdef CONFIG_SERVER2
    Self.connfd = fd;
    RESETCMD(&Self.cmdhead);
    Self.cmdhead.stat = STAT_READY;
    Self.cmdhead.next = NULL;
    stream_forward(fd, &Self.in, "r");
    stream_forward(fd, &Self.out, "w");
    stream_forward(fd, &Self.err, "w");
    np_switch_to(selfid);
#endif /* CONFIG_SERVER2 */
    
    dprintf(1, "npclient: id=%d, UsrLst[%d].pid=%d\n", Self.id, id, Self.pid);
    printf("%s%s%s", SEPERATOR, BANNERMSG, SEPERATOR);
    
    msg_init();
    upipe_init();
    env_init();
    
    // welcome msg
    Self.msg.dst_id = -1;
    Self.msg.type = MSG_TYPE_SYS;
    snprintf((char*)Self.msg.message, 1024, PERUSRMSG(ipmsg));
    msg_send(&Self.msg, 1);
    
    return 0;
}

int npclient_who(int argc, char *argv[])
{
    int id = 1;
    
    fprintf(stdout, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    FOR_EACH_USR(id){
        if (UsrLst[id].stat == USTAT_USED) {
            fprintf(stdout, "%d\t%s\t%s\t%s", id, 
                    UsrLst[id].name, 
                    UsrLst[id].netname,
                    id == selfid?"<-me\n":"\n" );
        }
    }
    return 0;
}

int npclient_name(int argc, char *argv[])
{
    int len;
    struct message *msg;
    if (argc != 2) {
        fprintf(stdout, "usage: name <your name>\n");
        return -1;
    }
    
    len = strlen(argv[1]);
    if (len+1 > 25) {
        fprintf(stdout, "error: name exceed namebuf\n");
        return -1;
    }
    
    // check if name exists
    for (size_t i = 1; i < MAXUSR; i++) {
        if (i == selfid || UsrLst[i].stat != USTAT_USED)
            continue;
        if (strcmp(UsrLst[i].name, argv[1]) == 0) {
            fprintf(stdout, "*** User '%s' already exists. ***\n", argv[1]);
            return -1;
        }
    }
    
    // set boardcast msg
    msg = &Self.msg;
    msg->dst_id = -1;
    snprintf((char*)msg->message, 1024, "*** User from %s is named '%s'. ***\n",
                                         Self.netname, argv[1]);
    
    // set name
    memcpy(Self.name, argv[1], len+1);
    
    // send msg
    msg_send(msg, 1);
    
    return 0;
}


int npclient_tell(int argc, char *argv[])
{
    struct message *msg = &Self.msg;
    int len;
    int recv_id;
    
    dprintf(20, "argc = %d, argv[1] = %s, argv[2] = %s\n",
                argc, argv[1], argv[2]);
    
    if (argc != 3) {
        fprintf(stdout, "usage: tell <id> <message>\n");
        return -1;
    }
    
    len = strlen(argv[2]);
    recv_id = atoi(argv[1]);
    
    if (usrchk(recv_id, NULL, 0) == -1) {
        return -1;
    }
    
    if (len >= 1024) {
        fprintf(stdout, "Error, message too long\n");
        return -1;
    }
    
    memcpy((char*)msg->message, argv[2], len+1);
    msg->dst_id = recv_id;
    msg->type = MSG_TYPE_TELL;
    msg_send(msg, 1);
    
    return 0;
}

int npclient_yell(int argc, char *argv[])
{
    struct message *msg = &Self.msg;
    int len = strlen(argv[1]);
    
    dprintf(20, "argc = %d, argv[1] = %s\n",
                argc, argv[1]);
                
    if (argc != 2) {
        fprintf(stdout, "usage: tell <id> <message>\n");
        return -1;
    }
    
    if (len >= 1024) {
        fprintf(stdout, "Error, message too long\n");
        return -1;
    }
    
    memcpy((char*)msg->message, argv[1], len+1);
    msg->dst_id = -1;
    msg->type = MSG_TYPE_YELL;
    msg_send(msg, 1);
    
    return 0;
}

#define SENDMSG "*** %s (#%d) just piped '%s' to %s (#%d) ***\n"
#define RECVMSG "*** %s (#%d) just received from %s (#%d) by '%s' ***\n"
int np_upipe_sysmsg(int id, int rw, char *cmd){
    int sid;
    int rid;
    struct message *msg = &Self.msg;
    char buf[1050];
    
    if (id == -1)
        return -1;
        
    // we dont need to check id here since `percmd_upipe` has checked for us
    
    for (size_t i = 0; i < strlen(cmd); i++) {
        if (cmd[i] == '\n' || cmd[i] == '\r') {
            cmd[i] = '\0';
            break;
        }
    }
    
    if (rw == 1) {
        sid = selfid;
        rid = id;
        snprintf(buf, 1024, SENDMSG, UsrLst[sid].name, sid, cmd, 
                                     UsrLst[rid].name, rid);
    }else{
        sid = id;
        rid = selfid;
        snprintf(buf, 1024, RECVMSG, UsrLst[rid].name, rid,
                                     UsrLst[sid].name, sid, cmd);
    }
    
    msg->dst_id = -1;
    msg->type = MSG_TYPE_SYS;
    memcpy((char*) msg->message, buf, strlen(buf)+1);
    
    msg_send(msg, 1);
    
    return 0;
}
#endif /* CONFIG_SERVER3 || CONFIG_SERVER2 */
