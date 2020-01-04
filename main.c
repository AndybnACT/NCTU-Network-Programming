#include "debug.h"
#include "socket.h"
#include "socks.h"
#include "firewall.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

int socks_server_exited(pid_t pid)
{
    return 0;
}

static void sigchld_hdlr(int sig, siginfo_t *info, void *ucontext)
{
    int rc;
    int status = 0;
    pid_t pid;
    
    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0)
            break;
        dprintf(0, "socks_server: pid %d has exit\n", pid);
        
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            dprintf(0, "exit code = %d\n", status);
            rc = socks_server_exited(pid);
            if (rc) {
                dprintf(1, "socks_server: cannot find active client with pid=%d\n",
                            pid);
                exit(-1);
            }
        }else if (WIFSIGNALED(status)) {
            status = WTERMSIG(status);
            dprintf(0, "signaled %d\n", status);
            rc = socks_server_exited(pid);
            if (rc) {
                dprintf(1, "socks_server: cannot find active client with pid=%d\n",
                            pid);
                exit(-1);
            }
        }else {
            dprintf(0, "sigchld_hdlr error\n");
        }
    }
}

int register_chld(void)
{
    int ret;
    struct sigaction sigdesc;
    sigdesc.sa_sigaction = sigchld_hdlr;
    sigdesc.sa_flags = SA_SIGINFO|SA_NOCLDSTOP|SA_RESTART;
    dprintf(1, "registering signal handler\n");
    
    ret = sigaction(SIGCHLD, &sigdesc, NULL);
    if (ret) {
        perror("sigaction");
        exit(-1);
    }
    return 0;
}

struct socks_client* socks_client_init(struct sockaddr_in *addrp)
{
    struct socks_client *client = (struct socks_client *) malloc(sizeof(struct socks_client));
    if (client == NULL) {
        perror("malloc");
        exit(-1);
    }
    
    inet_ntop(AF_INET, &addrp->sin_addr, client->ipstr, INET_ADDRSTRLEN);
    client->stat = 0;
    client->port = ntohs(addrp->sin_port);
    client->dstname = NULL;
    client->dstport = 0;
    client->dstfd = -1;
    client->resolved = NULL;
    memset(&client->reply, 0, sizeof(socks_reply));
    client->reply.CD = REPLY_REJECTED;
    
    return client;
}

int main(int argc, char const *argv[]) {
    int rc;
    long portl;
    int sockfd, connfd;
    pid_t child;
    struct sockaddr_in clientaddr;
    struct socks_client *socks_client;
    
    dprintf(0, "socks_server: start, pid=%d\n", getpid());
    
    // argument check
    if (argc != 2) {
        printf("Error, please specify one port number as cmd input\n");
        exit(-1);
    }
    
    // get portno from argv
    portl = strtol(argv[1], NULL, 10);
    if (!portl) {
        printf("Error cannot convert %s to port number\n", argv[1]);
        exit(-1);
    }
    
    register_chld();
    
    sockfd = socket_init((uint16_t) portl);
    
    while (1) {
        connfd = socket_accept(sockfd, &clientaddr);
        
        socks_client = socks_client_init(&clientaddr);        
        
        dprintf(0, "socks_server: connection from %s:%hu\n", socks_client->ipstr,
                socks_client->port);
        
        firewall_init();
        
retry_fork:
        child = fork();
        if (child == -1) {
            perror("fork");
            goto retry_fork;
        }else if (child == 0) {
            close(sockfd);
            dprintf(1, "socks_server: fork, child pid = %d\n", getpid());
            
            socks4_start(connfd, socks_client);
            
            exit(-1);
        }
        close(connfd);
    }
    
    return 0;
}
