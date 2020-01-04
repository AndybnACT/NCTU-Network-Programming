#include <netinet/in.h>
#include <sys/types.h>

#define MAXCONN 10

int socket_init(uint16_t port);
int socket_accept(int sockfd, struct sockaddr_in *addrp);
