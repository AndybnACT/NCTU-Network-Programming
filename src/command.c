#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "command.h"


#define DELIMITER "\n\t\r "
struct token {
    int type;
    int len;
    char *name;
    struct token *next;
};

int gettoken(char *buf, struct token *token)
{
    int tokcnt = 0;
    
    while (1) {
        while (strchr(DELIMITER, *buf) && *buf) {
            *buf = '\0';
            buf++;
        }
        // DEBUG !! when input is only \n
        if (!*buf) 
            break;
            
        token->name = buf;
        token->len = 0;
        tokcnt++;
        
        while (!strchr(DELIMITER, *buf) && *buf) {
            buf++;
            token->len++;
        }
        
        token->next = (struct token*) malloc(sizeof(struct token));
        token = token->next;
        token->name = NULL;
        token->len  = 0;
        token->next = NULL;
    }
    
    return tokcnt;
}

int npipe_getn(struct token *t)
{
    char *pipestr = t->name;
    int number;
    
    if (t->len == 1) {
        number = 0;
    }else{
        errno = 0;
        number = (int) strtol(pipestr+1, NULL, 10);
        if (errno) {
            printf("Error, unable to parse numbered pipe %s\n", pipestr);
            perror("strtol");
            exit(-1);
        }
        
        if (number <= 0 || number > 15000) {
            printf("Error, invalid number %d\n", number);
            exit(-1);
        }
    }
    
    return number;
}

int npipe_setcmd(int number, struct Command *head, int pipe_stderr)
{
    struct Command *cur = head;

    dprintf(2, "piping to %d th commands later\n", number);
    
    for (size_t i = 0; i < (size_t)number; i++) {
        if (!cur->next) 
            cur->next = zallocCmd();
        cur = cur->next;
    }
    
    head->cmd_out_pipe = cur;
    if (pipe_stderr)
        head->cmd_err_pipe = cur;

    if (!cur->cmd_first_in_pipe) 
        cur->cmd_first_in_pipe = head;
    
    return 0;
}

#define CMDSEP "|>!"

struct token * tok2npipe(struct token *tokp, struct Command *head)
{
    int pipelen;
    int errredir = 0;
    switch (*tokp->name) {
        case '>':
            tokp = tokp->next;
            if (!tokp->name) {
                printf("Error, stdout file redirection to nothing\n");
                exit(-1);
            }
            head->file_out_pipe = strdup(tokp->name);
            tokp = tokp->next;
            break;
        case '!':
            errredir = 1;
        case '|':
            pipelen = npipe_getn(tokp);
            tokp = tokp->next;
            if (pipelen == 0) {
                if (!tokp->name) {
                    printf("Error, pipe to nothing\n");
                    exit(-1);
                }
                npipe_setcmd(1, head, errredir);
            }else{
                npipe_setcmd(pipelen, head, errredir);
            }
            break;
        default:
            printf("Error, no such seperator %c\n", *tokp->name);
            exit(-1);
    }
    return tokp;
}

int flushCmd(struct Command *cur)
{
    struct Command *p = cur;
    dprintf(2, "flusing cmds...\n");
    while (p) {
        switch (p->stat) {
            case STAT_SET:
                dprintCmd(2, p);
                p->stat = STAT_READY;
                p->pid = 0;
                p->argc = 0;
                // leaking argv
                p->_func = NULL;
                p->file_out_pipe = NULL;
                // if pipe hasn't been allocated (npipe senario)
                if (p->pipes[0] == -1 && p->pipes[1] == -1) {
                    p->cmd_first_in_pipe = NULL;
                    p->fds[0] = -1;
                }
                p->fds[1] = -1;
                p->fds[2] = -1;
                p->cmd_out_pipe = NULL;
                p->cmd_err_pipe = NULL;
            case STAT_READY:
                break;
            default:
                printf("BUG invalid cmd stat when parsing cmd, check debug\n");
                dprintCmd(1, p);
                exit(-1);
        }
        p = p->next;
    }
    return 0;
}


struct Command * parse2Cmd(char *cmdbuf, size_t bufsize, struct Command *head)
{
    int ret;
    struct token tokenlist;
    struct token *tokptr = &tokenlist;
    struct Command *curcmd;
    
    // walk to the first command struct that hasn't been executed
    for (; head && head->stat != STAT_READY; head = head->next) {
        // dprintf(1, "walking through...\n");
        // dprintCmd(1, head);
    }
    curcmd = head;
    
    if (!head) {
        printf("Error, no available Command struct\n");
        exit(-1);
    }
    
    ret = gettoken(cmdbuf, &tokenlist);
    if (!ret)
        return curcmd;
    
    dprintf(1, "token list[%d]:\n", ret);
    for (struct token *tokenp = &tokenlist; tokenp->next; tokenp = tokenp->next) {
        dprintf(1, "\tname = %s, len = %d\n", tokenp->name, tokenp->len);    
    }
    
    while (1) {
        int argc = 0;
        struct token *tmptokp;
        if (!tokptr->name)
            break;
        
        tmptokp = tokptr;
        head->exec = strdup(tokptr->name);    
        
        // get argc
        while (!strchr(CMDSEP, *tmptokp->name)) {
            argc++;
            tmptokp = tmptokp->next;
            if (!tmptokp->name) // end of command
                break;
        }
        dprintf(1, "\t==> argc = %d\n", argc);
        if (!argc) {
            printf("parse error near \'%s\'\n", tmptokp->name);
            flushCmd(curcmd);
            return curcmd;
        }
        
        // alloc and fill argv accordingly
        head->argv = (char**) malloc((argc+1)*sizeof(char *));
        for (size_t i = 0; i < argc; i++) {
            head->argv[i] = strdup(tokptr->name);
            tokptr = tokptr->next;
        }
        head->argv[argc] = NULL;
        head->argc = argc;
        
        if (tmptokp->name) // must be '!' or '|' or '>'
            tmptokp = tok2npipe(tmptokp, head);
        
        head->stat = STAT_SET; 
        if (!head->next)
            head->next = zallocCmd(); 
        head = head->next;
        
        tokptr = tmptokp;
    }
    return curcmd;
}