#ifndef __USER_H__
#define __USER_H__

typedef struct _wb_user {
    int fd;
    char *uid;
    MDF *callback;

    struct _wb_user *next;
} WB_USER;

WB_USER* user_add(WB_USER *next, int fd, char *uid, MDF *callback);
void     user_destroy(WB_USER *user);

#endif
