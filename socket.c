#include "socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <stdint.h>

#include <arpa/inet.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/types.h>

int socket_init(uint16_t port)
{
    int sockfd;
    int rc;
    struct sockaddr_in servaddr;
    const int optval = 1;
    const socklen_t optlen = sizeof(optval);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(-1);
    }
    rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*) &optval, optlen);
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port); 
    rc = bind(sockfd, (struct sockaddr*) &servaddr, sizeof(struct sockaddr_in));
    if (rc) {
        perror("bind");
        exit(-1);
    }
    
    rc = listen(sockfd, MAXCONN);
    if (rc) {
        perror("listen");
        exit(-1);
    }
        
    return sockfd;
}

int socket_accept(int sockfd, struct sockaddr_in *addrp)
{
    int connfd;
    uint32_t len = sizeof(struct sockaddr_in);

retry:    
    connfd = accept(sockfd, (struct sockaddr*) addrp, &len);
    if (connfd == -1) {
        perror("accept");
        if (errno == EINTR) {
            goto retry;
        }
        exit(-1);
    }
    
    return connfd;
}