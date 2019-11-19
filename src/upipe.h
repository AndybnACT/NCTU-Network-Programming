#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif


#if defined(CONFIG_SERVER3) || defined(CONFIG_SERVER2)
int upipe_init(void);
int upipe_get_readend(int srcid);
int upipe_get_writeend(int dstid);
int upipe_set_writeend(int dstid, int force);
int upipe_release(int srcid);
#endif /* CONFIG_SERVER3 || CONFIG_SERVER2 */
