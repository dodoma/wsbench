#ifndef __USER_H__
#define __USER_H__

typedef struct _wb_user {
    int fd;
    char *uid;
    char *ticket;
    MDF *roomnode;
    MDF *callback;

    unsigned char *rcvbuf;
    size_t rcvlen;
    size_t excess;

    int opcode;
    unsigned char *payload;
    uint64_t payloadsize;

    struct _wb_user *next;
} WB_USER;

WB_USER* user_add(WB_USER *next, int fd, char *uid, char *ticket,
                  MDF *roomnode, MDF *callback, int usersn);
void     user_destroy(WB_USER *user);

#endif
