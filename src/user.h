#ifndef __USER_H__
#define __USER_H__

typedef struct _wb_user {
    int fd;

    int usersn;
    char *uid;
    char *ticket;
    MDF *sitenode;              /* public memory */
    MDF *callback;              /* private memory */

    struct _wb_room *room;

    unsigned char *rcvbuf;
    size_t rcvlen;
    size_t excess;

    int opcode;
    unsigned char *payload;
    uint64_t payloadsize;

    struct _wb_user *next;
} WB_USER;

typedef struct _wb_room {
    uint32_t turncount;
    uint32_t usercount;
    MDF *inode;
    int state;

    WB_USER *user;

    /* heartbeat */
    time_t hb_last;
    int    hb_interval;
    char  *hb_message;

    struct _wb_room *next;
} WB_ROOM;

enum {
    ROOM_STATE_INIT = 0,
    ROOM_STATE_READY,
    ROOM_STATE_RUNNING,
    ROOM_STATE_FAILURE,
    ROOM_STATE_GAMEOVER,
    ROOM_STATE_CLOSED
};

WB_ROOM* user_room_open(WB_ROOM *next, char *uid, char *ticket,
                        MDF *sitenode, int usersn);
bool user_room_add(WB_ROOM *room, char *uid, char *ticket,
                   MDF *sitenode, int usersn, int usernumber);

void user_room_check(WB_ROOM *room, int efd);
void user_room_destroy(WB_ROOM *room);

#endif
