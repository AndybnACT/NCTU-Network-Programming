#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif


#ifdef CONFIG_SERVER1
int upipe_init(void){return -1;}
int initialize_fifo(int dstid, char *path){return -1;}
int upipe_get_readend(int srcid){return -1;}
int upipe_get_writeend(int dstid){return -1;}
int upipe_set_writeend(int dstid, int force){return -1;}
int upipe_release(int srcid){return -1;}
#endif /* CONFIG_SERVER1 */


#ifdef CONFIG_SERVER3
int upipe_init(void);
int initialize_fifo(int dstid, char *path);
int upipe_get_readend(int srcid);
int upipe_get_writeend(int dstid);
int upipe_set_writeend(int dstid, int force);
int upipe_release(int srcid);
#endif /* CONFIG_SERVER3 */
