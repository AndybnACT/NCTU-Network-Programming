#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif
#include "npshell.h"
#include "command.h"
#include "debug.h"
#include "net.h" /* MAXUSR macro */
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef CONFIG_SERVER3
static void sigchld_hdlr(int sig, siginfo_t *info, void *ucontext)
{
    int rc;
    int status = 0;
    pid_t pid;
    
    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0)
            break;
        dprintf(0, "npserver: pid %d has exit\n", pid);
        
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            dprintf(0, "exit code = %d\n", status);
            rc = npserver_reap_client(pid);
            if (rc) {
                dprintf(1, "npserver: cannot find active client with pid=%d\n",
                            pid);
                exit(-1);
            }
        }else if (WIFSIGNALED(status)) {
            status = WTERMSIG(status);
            dprintf(0, "signaled %d\n", status);
            rc = npserver_reap_client(pid);
            if (rc) {
                dprintf(1, "npserver: cannot find active client with pid=%d\n",
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
    sigdesc.sa_flags = SA_SIGINFO|SA_NOCLDSTOP;
    dprintf(1, "registering signal handler\n");
    
    ret = sigaction(SIGCHLD, &sigdesc, NULL);
    if (ret) {
        perror("sigaction");
        exit(-1);
    }
    return 0;
}
#endif /* CONFIG_SERVER3 */

int stream_forward(const int fd, FILE **stream_ptr, char *mode)
{
    int rc, newfd;
    
    newfd = fileno(*stream_ptr);
    dprintf(0, "newfd = %d, oldfd = %d\n", newfd, fd);
    fclose(*stream_ptr);
    rc = dup2(fd, newfd);
    if (rc == -1) {
        perror("dup2");
    }
    
    *stream_ptr = fdopen(newfd, mode);
    if (!(*stream_ptr)) {
        perror("fdopen");
        exit(-9);
    }
    rc = setvbuf(*stream_ptr, NULL, _IONBF, 0);
    if (rc) {
        perror("setvbuf");
        exit(-9);
    }
    
    return 0;
}

int main(int argc, char const *argv[]) {
    pid_t child;
#ifdef CONFIG_SERVER1
    int chld_stat;
#endif
    long portl;
    int rc;
#ifdef CONFIG_SERVER3
    int shmfd;
#endif
    int sockfd;
    int connfd;
    struct sockaddr_in servaddr, clientaddr;
    socklen_t len;
    char ipbuf[150];
    char ipmsgbuf[200];
    
    const int optval = 1;
    const socklen_t optlen = sizeof(optval);
    dprintf(0, "np_ccos start, pid=%d\n", getpid());
    
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
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(-1);
    }
    rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*) &optval, optlen);
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons((short)portl); 
    rc = bind(sockfd, (struct sockaddr*) &servaddr, sizeof(struct sockaddr_in));
    if (rc) {
        perror("bind");
        exit(-1);
    }
    
#ifdef CONFIG_SERVER3
    shmfd = npserver_init();
    register_chld();
#endif /* CONFIG_SERVER3 */
    
    while (1) {
        rc = listen(sockfd, MAXUSR);
        if (rc) {
            perror("listen");
            exit(-1);
        }
        
        dprintf(0, "server waiting on port %d\n", (short) portl);
        len = sizeof(struct sockaddr_in);
retry_accept:
        connfd = accept(sockfd, (struct sockaddr*) &clientaddr, &len);
        if (connfd == -1) {
            perror("accept");
            if (errno == EINTR)
                goto retry_accept; 
            exit(-1);
        }
retry_fork:
        child = fork();
        if (child == -1) {
            perror("fork");
            goto retry_fork;
        }else if (child == 0) {
            inet_ntop(AF_INET, &clientaddr.sin_addr, ipbuf, INET_ADDRSTRLEN);
            snprintf(ipmsgbuf, 150, "%s:%hu", ipbuf, ntohs(clientaddr.sin_port));
            dprintf(0, "client coming from %s\n", ipmsgbuf);
            
            dprintf(0, "forwarding std streams to the incoming port\n");
            stream_forward(connfd, &stdin,  "r");
            stream_forward(connfd, &stdout, "w");
            stream_forward(connfd, &stderr, "w");
            close(sockfd);
            close(connfd); // connfd has already forwarded to 0, 1, 2 
            
#ifdef CONFIG_SERVER3
            npclient_init(shmfd, ipmsgbuf);
#endif /* CONFIG_SERVER3 */

            npshell(argc, argv);
            exit(-1);
        }
        
        close(connfd);
        
#ifdef CONFIG_SERVER1
        dprintf(0, "%d fork'ed\n", child);
        do {
            rc = waitpid(child, &chld_stat, 0);
            if (child != rc) {
                perror("waitpid");
                exit(-1);
            }
            dprintf(1, "waitpid-stat = %d\n", child);
        } while(!WIFEXITED(chld_stat));
        
        dprintf(0, "npshell exit status = %d\n", WEXITSTATUS(chld_stat));
#endif /* CONFIG_SERVER1 */
    }

    return 0;
}