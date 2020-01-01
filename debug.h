#include <stdio.h>
#define CONFIG_DEBUG

#ifdef CONFIG_DEBUG

#define CONFIG_DEBUG_LVL 10

#define dprintf(lvl, args...){      \
    if (lvl < CONFIG_DEBUG_LVL) {   \
        fprintf(stderr, ##args);    \
    }                               \
}

#else

#define CONFIG_DEBUG_LVL 0
#define dprintf(lvl, args...) { }


#endif /* DEBUG */
