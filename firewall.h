#include <stdint.h>

#define FW_PASS         0x0
#define FW_EACCESS      0x1

#define FWRULE_BIND      0x1
#define FWRULE_CONNECT   0x2   

struct fw_rule {
    uint32_t ip;
    uint32_t mask;
    uint8_t mode;
    struct fw_rule *next;
};

int firewall_init(void);
int firewall_rule(uint cmd, uint32_t net_ip);