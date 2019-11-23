#ifndef CONFIG
#include "_config.h"
#define CONFIG
#endif


#if defined(CONFIG_SERVER3) || defined(CONFIG_SERVER2)
#define ERR_PIPE_EXIST   -2
#define ERR_NOUSR        -3
#define ERR_PIPENOTFOUND -4
#define PIPE_EXIST "*** Error: the pipe #%d->#%d already exists. ***\n"
#define PIPENOTFOUND "*** Error: the pipe #%d->#%d does not exist yet. ***\n"
int upipe_init(void);
int upipe_get_readend(int srcid);
int upipe_get_writeend(int dstid);
int upipe_set_writeend(int dstid, int force);
int upipe_release_all(int id);
int upipe_release(int srcid);
#endif /* CONFIG_SERVER3 || CONFIG_SERVER2 */
