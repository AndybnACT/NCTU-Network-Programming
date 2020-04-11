#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif

#ifdef CONFIG_SERVER2
#include "command.h"
#include "debug.h"
#include "net.h"
#include "msg.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int find_client_by_fd(int fd)
{
    int id = 1;
    FOR_EACH_USR(id){
        if (UsrLst[id].connfd == fd) {
            return id;
        }
    }
    printf("cannot find usr by connfd=%d\n", fd);
    return 0;
}



extern struct Command *Cmd_Head;
int np_switch_to(int id)
{
    selfid = id;
    stdin = UsrLst[id].in;
    stdout = UsrLst[id].out;
    stderr = UsrLst[id].err;
    Cmd_Head = &Self.cmdhead;
    return 0;
}
// np_single_proc is a function of no return for server2
int np_single_proc(int sockfd)
{
    int rc;
    int selcnt;
    int newfd;
    int id;
    socklen_t len;
    fd_set read_fd_set, active_fd_set;
    struct sockaddr_in clientaddr;
    char ipbuf[150];
    char ipmsg[200];
    
    rc = listen(sockfd, MAXUSR);
    if (rc) {
        perror("listen");
        exit(-1);
    }
    
    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);
    
    npserver_init();
    npshell_init();
    
    while (1) {
        read_fd_set = active_fd_set;
retry:
        selcnt = select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);
        if (selcnt == -1) {
            if (errno == EINTR) {
                goto retry;
            }
            perror("select");
            exit(errno);
        }
        dprintf(0, "select, %d fd waiting on ...", selcnt);
        for (size_t i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &read_fd_set)) {
                dprintf(0, " fd = %zu\n", i);
                if (i == sockfd) {
                    len = sizeof(struct sockaddr_in);
                    newfd = accept(sockfd, (struct sockaddr*) &clientaddr, &len);
                    if (newfd == -1) {
                        perror("accept");
                        exit(errno);
                    }
                    dprintf(0, "adding %d to active_fd_set\n", newfd);
                    FD_SET(newfd, &active_fd_set);
                    
                    inet_ntop(AF_INET, &clientaddr.sin_addr, ipbuf, INET_ADDRSTRLEN);
                    snprintf(ipmsg, 150, "%s:%hu", ipbuf, ntohs(clientaddr.sin_port));
                    dprintf(0, "client coming from %s\n", ipmsg);
                    
                    npclient_init(newfd, ipmsg);// switch occurs inside this func
                    printf("%% ");
                }else {
                    id = find_client_by_fd(i);
                    np_switch_to(id);
                    npshell_exec_once();
                    
                    if (Self.exit == 1) {
                        dprintf(0, "reaping client id = %d\n", selfid);
                        npserver_reap_client(selfid);
                        FD_CLR(i, &active_fd_set);
                    }else {
                        printf("%% ");
                    }
                }
                np_switch_to(0);
                selcnt--;
            }
        }
    }
    exit(0);
}

#endif /* CONFIG_SERVER2 */
