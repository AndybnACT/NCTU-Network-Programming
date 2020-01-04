#define CONFIG_DEBUG
#include "debug.h"
#include "socks.h"
#include "firewall.h"

#include <sys/inotify.h>
#include <poll.h>

#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define FW_FILE "socks.conf"


#define TOK_PERMIT "permit"

int inotify_fd = -1;
struct pollfd conf_file;
struct inotify_event *event;

struct fw_rule *FW_Rules;
#define INIT_RULE(rule) {   \
    (rule)->ip = 0;         \
    (rule)->mask = 0;       \
    (rule)->mode = 0;       \
    (rule)->next = NULL;    \
}

struct fw_rule* rule_alloc()
{
    struct fw_rule *r = (struct fw_rule*) malloc(sizeof(struct fw_rule));
    INIT_RULE(r);
    return r;
}

int parse_fwip(struct fw_rule *fwbuf, char *ipstr)
{
    long rc;
    int cnt = 0;
    int ip[4] = {0,};
    int mask[4] = {0,};
    char *end;
    
    dprintf(4, "parsing firewall ip\n");
    for (size_t i = 0; i < 4; i++) {
        
        while (*ipstr == ' ') {
            ipstr++;
            cnt++;
        }
        if (*ipstr == '.') {
            ipstr++;
            cnt++;
        }else if (i != 0){
            return -1;
        }
        while (*ipstr == ' ') {
            ipstr++;
            cnt++;
        }
        
        if (*ipstr == '*') {
            mask[i] = 0xFF;
            ip[i] = 0;
            dprintf(4, "ip[%zu] = masked\n", i);
            ipstr++;
            cnt++;
            continue;
        }
        
        errno = 0;
        rc = strtol(ipstr, &end, 10);
        if (ipstr == end) {
            dprintf(4, "cannot make conversion to ip\n")
            return -1;
        }
        ip[i] = rc;
        dprintf(4, "ip[%zu] = %d\n", i, ip[i]);
        
        cnt += end-ipstr;
        ipstr = end; 
    }
    
    fwbuf->ip = 0;
    fwbuf->mask = 0;
    for (size_t i = 0; i < 4; i++) {
        if (ip[i] > 255) {
            return -1;
        }
        // big-endian representation of ip
        fwbuf->ip   |= ip[i]   << (3 - i)*8;
        fwbuf->mask |= mask[i] << (3 - i)*8;
    }
    fwbuf->ip   = ntohl(fwbuf->ip);
    fwbuf->mask = ntohl(fwbuf->mask);
    
    return cnt;
}

int fw_parse(int fd)
{
    int rc;
    char *buf;
    off_t off;
    size_t cur = 0;
    char *found;
    struct fw_rule rulebuf = {0,};
    struct fw_rule *cur_rule = FW_Rules;
    enum  stat {INI, COMMENT, RULE, IP, END} stat = INI;
    
    off = lseek(fd, 0, SEEK_END);
    if (off == -1) {
        perror("lseek");
        goto general_err;
    }
    rc = lseek(fd, 0, SEEK_SET);
    if (rc) {
        perror("lseek");
        goto general_err;
    }
    
    buf = (char*) malloc(off);
    if (!buf) {
        perror("malloc");
        goto general_err;
    }
    
    rc = read(fd, buf, off);
    if (rc != off) {
        dprintf(2,"!!! conf file not completely read (%d, %ld)\n", 
                rc, off);
    }
    
    while (cur < (size_t) off) {
        if (buf[cur] == '#' && stat == INI) {
            stat = COMMENT;
        }
        if (buf[cur] == ' ' || buf[cur] == '\t') {
            cur++;
            continue;
        }
        switch (stat) {
            case COMMENT:
                if (buf[cur] == '\n') {
                    stat = INI;
                }
                break;
            case INI:
                if (buf[cur] == '\n') {
                    break;
                }
                INIT_RULE(&rulebuf);
                found = strstr(buf+cur, TOK_PERMIT);
                if (found && found == buf+cur) {
                    cur = found - buf + strlen(TOK_PERMIT);
                    stat = RULE;
                }else{
                    goto parse_err;
                }
                break;
            case RULE:
                if (buf[cur] == 'b') {
                    rulebuf.mode = FWRULE_CONNECT;
                }else if (buf[cur] == 'c') {
                    rulebuf.mode = FWRULE_BIND;
                }else{
                    dprintf(3, "rule not recognized\n");
                    goto parse_err;
                }
                stat = IP;
                break;
            case IP:
                rc = parse_fwip(&rulebuf, buf+cur);
                
                if (rc == -1) {
                    dprintf(3, "error parsing ip\n");
                    goto parse_err;
                }else {
                    cur += rc;
                }
                
                *cur_rule = rulebuf;
                cur_rule->next = rule_alloc();
                cur_rule = cur_rule->next;
                
                stat = INI;
                break;
            default:
                goto parse_err;
        }
        cur++;
    }
    if (stat == INI) {
        dprintf(3, "firewall list parse completed\n");
        return 0;
    }
    
parse_err:
    dprintf(3, "syntax error at %zu, stat=%d\n", cur, stat);
general_err:
    return -1;
}

int firewall_init(void)
{
    int rc;
    int fd;
    
    if (inotify_fd == -1) {
        fd = inotify_init();
        if (fd == -1) {
            perror("inotify_init");
            goto firewall_err;
        }
        inotify_fd = fd;
        
        rc = inotify_add_watch(inotify_fd, FW_FILE, IN_MODIFY);
        if (rc == -1) {
            perror("inotify_add_watch");
            goto firewall_err;
        }
        
        conf_file.fd = inotify_fd;
        conf_file.events = POLLIN;
        event = (struct inotify_event *)
                malloc(sizeof(struct inotify_event) + 255 + 1);
        if (!event) {
            perror("malloc");
            goto firewall_err;
        }
    }else{
        rc = poll(&conf_file, 1, 0);
        if (rc == -1) {
            perror("poll");
            goto firewall_err;
        }else if (rc == 0) {
            dprintf(1, "firewall is up to date\n");
            return 0;
        }
        rc = read(inotify_fd, event, sizeof(struct inotify_event) + 255 + 1);
        if (rc == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        rc = inotify_add_watch(inotify_fd, FW_FILE, IN_MODIFY);
        if (rc == -1) {
            perror("inotify_add_watch");
            goto firewall_err;
        }    
    }
    
    // open config file
    fd = open(FW_FILE, O_RDONLY);
    if (fd == -1) {
        perror("open");
        goto firewall_err;
    }
    
    FW_Rules = rule_alloc();
    
    // parse to rules
    fw_parse(fd);
    
    close(fd);
    
    return 0;
    
firewall_err:
    close(inotify_fd);
    inotify_fd = -1;
    return -1;
}

int firewall_rule(uint cmd, uint32_t net_ip)
{
    int reply = -FW_EACCESS;
    uint32_t cmp;
    
    for (struct fw_rule *r = FW_Rules; r->next; r = r->next) {
        if (cmd == r->mode) {
            cmp = net_ip & (~(r->mask));
            if (cmp == r->ip) {
                return FW_PASS;
            }
        }
    }
    
    return reply;
}