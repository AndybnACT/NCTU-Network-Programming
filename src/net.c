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

#ifdef CONFIG_SERVER3
#include "command.h"
#include "msg.h"
#include "upipe.h"

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


static int np_shmem_init(int shmfd, int prot)
{
    shmem_obj.base = mmap(NULL, ROUNDUP(USRSIZE), prot, MAP_SHARED, shmfd, 0);
    if (shmem_obj.base == MAP_FAILED)
        return -1;
    shmem_obj.top  = (void*)((char*)shmem_obj.base + USRSIZE);
    shmem_obj.limit= (void*)((char*)shmem_obj.base + ROUNDUP(USRSIZE));
    return 0;
}

int npserver_init()
{
    int rc;
    int fd;
    
    fd = shm_open("npshell_shared", O_RDWR|O_CREAT|O_TRUNC, 0666);
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
    for (int i = 0; i < MAXUSR; i++) {
        RESET_USR(&UsrLst[i]);
        UsrLst[i].id = i;
    }
    
    // id 0 is reserved for server
    UsrLst[0].stat = USTAT_USED;
    UsrLst[0].pid  = getpid();
    strcpy(UsrLst[0].name, "npserver");
    selfid = 0;
    
    
    dprintf(1, "npserver: shared memory mapped @ %p, current top = %p, limit = %p\n",
                shmem_obj.base, shmem_obj.top, shmem_obj.limit);
    
    return fd;
}

#define LEAVE_MSG(n) "*** User ’%s’ left. *** ", (n)

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
            
            RESET_USR(&UsrLst[killid]);
            return 0;
        }
    }
    return -1;
}

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
#define PERUSRMSG(ipbuf) "*** User ’(no name)’ entered from %s. ***\n", (ipbuf)

int npclient_init(int fd, char *ipmsg)
{
    int id;
    
    id = client_initialize_self();
    memcpy(Self.netname, ipmsg, strlen(ipmsg));
    
    dprintf(1, "npclient: id=%d, UsrLst[%d].pid=%d\n", Self.id, id, Self.pid);
    printf("%s%s%s", SEPERATOR, BANNERMSG, SEPERATOR);
    
    msg_init();
    upipe_init();
    
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
    
    // set boardcast msg
    
    // set name
    memcpy(Self.name, argv[1], len+1);
    
    // send msg
    
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
    
    if (recv_id < 1 || recv_id > MAXUSR) {
        printf("invalid receiver id for input: %s\n", argv[1]);
        return -1;
    }
    if (UsrLst[recv_id].stat != USTAT_USED) {
        printf(USRNOTFOUND(recv_id));
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
#endif /* CONFIG_SERVER3 */
