#include "debug.h"
#include "socks.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/select.h>
#include <sys/time.h>

char *getstr(int fd)
{
    char buf[255];
    int i = 0;
    int rc;
    while (i < 255) {
        rc = read(fd, buf+i, 1);
        if (rc == -1) {
            perror("getstr read");
            exit(-1);
        }
        if (buf[i] == '\0') {
            return strdup(buf);
        }
        i += rc;
    }
    dprintf(3, "buf of getstr is full!! \n");
    return NULL;
}

int blocked_read(int fd, char *buf, size_t len)
{
    int rc;
    while (len) {
        rc = read(fd, buf, len);
        if (rc == -1) {
            perror("read");
            return len;
        }
        len -= rc;
        buf += rc;
    }
    return 0;
}

int blocked_write(int fd, char *buf, size_t len)
{
    int rc;
    while (len) {
        rc = write(fd, buf, len);
        if (rc == -1) {
            perror("write");
            return len;
        }
        len -= rc;
        buf += rc;
    }
    return 0;
}

int socks4_parse_header(int fd, struct socks_client *client)
{
    char *usrid;
    char *dst;
    uint32_t ip_h, ip_n;
    uint16_t port_h;
    struct socks4_req_header *req = malloc(sizeof(struct socks4_req_header));
    
    // head section
    if (blocked_read(fd, (char*) &req->head, sizeof(socks_head))) {
        return REPLY_REJECTED;
    }
    
    // basic checks
    if (req->head.VN != 4) {
        fprintf(stderr, "Error, unsupported version number: %d\n", req->head.VN);
        exit(-1);
    }
    
    switch (req->head.CD) {
        case 1:
            client->stat |= SOCKS_CONNECT;
            break;
        case 2:
            client->stat |= SOCKS_BIND;
            break;
        default:
            printf("Error, unsupported command type: %d\n", req->head.CD);
            return REPLY_REJECTED;
    }
    
    // parse
    usrid = getstr(fd);
    if (usrid == NULL) {
        dprintf(1, "Warning, Unknown user id!\n");
    }
    req->userid = usrid;
    req->uidlen = usrid ? strlen(usrid):0;
    dprintf(2, "user-id = %s\n", usrid);
    
    ip_h = ntohl(req->head.DSTIP);
    ip_n = htonl(ip_h);
    port_h = ntohs(req->head.DSTPORT);
    
    if (ip_h < 256) { // 0.0.0.x
        dprintf(1, "socks 4a\n");
        dst = getstr(fd);
        if (dst == NULL) {
            fprintf(stderr, "Error, cannot get domain name!\n");
            exit(-1);
        }
        req->domain = dst;
        req->domainlen = strlen(dst);
        
        client->stat |= SOCKS_4A;
    }else {
        dprintf(1, "socks 4\n");
        dst = (char*) malloc(INET_ADDRSTRLEN);
        if (dst == NULL) {
            perror("malloc");
            exit(-1);
        }
        inet_ntop(AF_INET, &(ip_n), dst, INET_ADDRSTRLEN);
        req->domain = NULL;
        req->domainlen = 0;
        
        client->stat |= SOCKS_4;
    }
    
    // to socks_client
    client->dstname = dst;
    client->dstport = port_h;
    client->request = req;
    
    dprintf(2, "[parsed] socks request:\n");
    dprintf(2, "\tdest: %s\n", client->dstname);
    dprintf(2, "\tport: %hu\n", client->dstport);
    dprintf(2, "\tcmd: 0x%x\n", (client->stat & SOCKS_CMD_MASK) >> 4);
    
    return REPLY_GRANTED;
}

int socks4_resolve(struct socks_client *client)
{
    int rc;
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    char portbuf[6];
    
    dprintf(3, "resolving\n");
    client->resolved = NULL;
    
    
    if ((client->stat & SOCKS_VER_MASK) != SOCKS_4 && 
        (client->stat & SOCKS_VER_MASK) != SOCKS_4A) {
        dprintf(3, "BUG, socks version not supported\n");
        return REPLY_REJECTED;
    }
    
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags |= AI_CANONNAME;
    
    
    snprintf(portbuf, 6, "%hu", client->dstport);
    rc = getaddrinfo(client->dstname, portbuf, &hint, &res);
    if (rc) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return REPLY_REJECTED;
    }
    
    client->resolved = res;
    return REPLY_GRANTED;
}

int socks4_firewall(struct socks_client *client)
{
    return REPLY_GRANTED;
}

int socks4_connect(struct socks_client *client)
{
    int rc;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo *addr = client->resolved;
    
    if (fd == -1) {
        perror("socket");
        return REPLY_REJECTED;
    }
    
    do {
        dprintf(3, "Trying to connect %s...\n", client->dstname);
        rc = connect(fd, addr->ai_addr, sizeof(struct addrinfo));
        if (rc == 0) {
            dprintf(1, "connected!\n");
            client->dstfd = fd;
            return REPLY_GRANTED;
        }
        addr = addr->ai_next;
    } while(addr);
    
    dprintf(3, "Fail to connect to %s with possible tries\n", client->dstname);
    
    return REPLY_REJECTED;
}

void socks4_reply(int fd, socks_reply *reply) 
{
    if (blocked_write(fd, (char*) reply, sizeof(socks_reply))) {
        dprintf(2, "Cannot send reply header\n");
        exit(EXIT_FAILURE);
    }
    dprintf(1, "reply header sent, code = 0x%x\n", reply->CD);
    return;
}

int socks4_relay_run(int fd1, int fd2)
{
    int rc;
    char buf[4096] = {0,};
    fd_set read_fd_set, active_fd_set;
    int nrsel;
    
    FD_ZERO(&active_fd_set);
    
    FD_SET(fd1, &active_fd_set);
    FD_SET(fd2, &active_fd_set);
    
    dprintf(2, "releying on fd1 = %d, fd2 = %d\n", fd1, fd2);
    while (1) {
        read_fd_set = active_fd_set;
        nrsel = select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);
        if (nrsel == -1) {
            if (nrsel == -1) {
                perror("select");
                rc = errno;
                break;
            }
        }
        for (size_t i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &read_fd_set)) {
                // dprintf(3, " fd = %zu\n", i);
                
                rc = read(i, buf, 4096);
                if (rc <= 0) {
                    dprintf(4, "---------> read failed, closing socket\n");
                    close(fd1);
                    close(fd2);
                    return 0;
                }
                
                if (i == fd1 && rc) {
                    rc = blocked_write(fd2, buf, rc);
                }
                if (i == fd2 && rc) {
                    rc = blocked_write(fd1, buf, rc);
                }
            }
        }
        
        
    }
    
    return rc;
}

int socks4_cmd_connect(int fd, struct socks_client *client)
{
    int rc;
    socks_reply *reply = &client->reply;
    reply->VN = 0;
    reply->CD = REPLY_REJECTED;
    reply->DSTIP = 0;
    reply->DSTPORT = 0;
    
    do {
        rc = socks4_resolve(client);
        if (rc != REPLY_GRANTED){
            dprintf(2, "fail to resolve host\n");
            break;
        }
        
        rc = socks4_firewall(client);
        if (rc != REPLY_GRANTED){
            dprintf(2, "connection violates firewall rules\n");
            break;
        }
        
        rc = socks4_connect(client);
        if (rc != REPLY_GRANTED){
            dprintf(2, "fail to connect host\n");
            break;
        }
        reply->CD = rc;
        socks4_reply(fd, reply);
    } while(0);
    
    // opt to socks4_start to reply on failure;
    return rc;
}

int socks4_cmd_bind(int fd, struct socks_client *client)
{
    return REPLY_REJECTED;
}

void socks4_start(int fd, struct socks_client *client)
{
    int rc;
    socks_reply failure;
    
    rc = socks4_parse_header(fd, client);
    if (rc == REPLY_GRANTED){
        switch (client->stat & SOCKS_CMD_MASK) {
            case SOCKS_CONNECT:
                rc = socks4_cmd_connect(fd, client);
                break;
            case SOCKS_BIND:
                rc = socks4_cmd_bind(fd, client);
                break;
            default:
                dprintf(1, "BUG, unsupported command type %d\n", 
                        (client->stat & SOCKS_CMD_MASK)>>4);
                rc = REPLY_REJECTED;
        }
        if (rc == REPLY_GRANTED){
            rc = socks4_relay_run(fd, client->dstfd);
            exit(rc);
        }
    }
    
    failure.CD = rc;
    socks4_reply(fd, &failure);
    exit(rc);
    return;
}
