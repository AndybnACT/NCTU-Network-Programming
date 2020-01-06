#include <stdio.h>
#include <unistd.h>
// #define CONFIG_DEBUG

#ifdef CONFIG_DEBUG

#define CONFIG_DEBUG_LVL 4

#define dprintf(lvl, args...){               \
    if (lvl < CONFIG_DEBUG_LVL) {            \
        fprintf(stderr, "[%d]:", getpid());  \
        fprintf(stderr, ##args);             \
    }                                        \
}

#else

#define CONFIG_DEBUG_LVL 0
#define dprintf(lvl, args...) { }


#endif /* DEBUG */
