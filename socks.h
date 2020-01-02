#include <stdint.h>
#include <netinet/in.h>

#define REPLY_GRANTED   90
#define REPLY_REJECTED  91


struct  __attribute__((__packed__)) __socks_raw{
    uint8_t VN;
    uint8_t CD;
    uint16_t DSTPORT;
    uint32_t DSTIP;
};

typedef struct  __attribute__((__packed__)) __socks_raw socks_head;
typedef struct  __attribute__((__packed__)) __socks_raw socks_reply;

struct socks4_req_header {
    socks_head head;
    uint32_t uidlen;
    char *userid;
    uint32_t domainlen;
    char *domain;
};

#define SOCKS_VER_MASK 0x00F
#define SOCKS_4        0x001
#define SOCKS_4A       0x002
#define SOCKS_CMD_MASK 0xFF0
#define SOCKS_CONNECT  0x010
#define SOCKS_BIND     0x020

struct socks_client {
    uint32_t stat;
    char ipstr[INET_ADDRSTRLEN];
    uint16_t port;
    char *dstname;
    struct addrinfo *resolved;
    uint16_t dstport;
    int dstfd;
    struct socks4_req_header *request;
    socks_reply *reply;
};

void socks4_start(int fd, struct socks_client *client);
