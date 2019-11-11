#include "npshell.h"
#include "command.h"
#include "debug.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int stream_forward(const int fd, FILE **stream_ptr, char *mode)
{
    int rc, newfd;
    
    newfd = fileno(*stream_ptr);
    fclose(*stream_ptr);
    dup2(fd, newfd);
    
    *stream_ptr = fdopen(newfd, mode);
    if (!stream_ptr) {
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
    int chld_stat;
    long portl;
    int rc;
    int sockfd;
    int connfd;
    struct sockaddr_in servaddr, clientaddr;
    socklen_t len;
    char ipbuf[150];
    
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
    while (1) {
        rc = listen(sockfd, 1);
        if (rc) {
            perror("listen");
            exit(-1);
        }
        
        child = fork();
        if (child == -1) {
            perror("fork");
            exit(-1);
        }else if (child == 0) {
            dprintf(0, "server waiting on port %d\n", (short) portl);
            
            len = sizeof(struct sockaddr_in);
            connfd = accept(sockfd, (struct sockaddr*) &clientaddr, &len);
            if (connfd == -1) {
                perror("accept");
                exit(-1);
            }
            
            inet_ntop(AF_INET, &clientaddr.sin_addr, ipbuf, INET_ADDRSTRLEN);
            dprintf(0, "client coming from %s\n", ipbuf);
            
            dprintf(0, "forwarding std streams to the incoming port\n");
            stream_forward(connfd, &stdin,  "r");
            stream_forward(connfd, &stdout, "w");
            stream_forward(connfd, &stderr, "w");
            close(sockfd);
            
            npshell(argc, argv);
            exit(-1);
        }
        
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
    }

    return 0;
}