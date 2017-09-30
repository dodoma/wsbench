#include "reef.h"

#include "user.h"
#include "poll.h"
#include "site.h"

static WB_USER* _user_add(char *uid, char *ticket, MDF *sitenode, int usersn)
{
    if (!uid || !ticket || !sitenode) return NULL;

    WB_USER *user = mos_calloc(1, sizeof(WB_USER));
    user->fd = -1;
    user->usersn = usersn;
    user->uid = strdup(uid);
    user->ticket = strdup(ticket);
    user->sitenode = sitenode;
    mdf_init(&user->callback);

    user->room = NULL;

    user->rcvbuf = NULL;
    user->rcvlen = 0;
    user->excess = 0;
    user->opcode = 0;
    user->payload = 0;
    user->payloadsize = 0;

    user->next = NULL;

    return user;
}

static void _user_destroy(WB_USER *user)
{
    while (user) {
        WB_USER *next = user->next;

        if (user->fd > 0) close(user->fd);

        mos_free(user->uid);
        mos_free(user->ticket);
        mdf_destroy(&user->callback);
        mos_free(user->rcvbuf);

        mos_free(user);

        user = next;
    }
}


WB_ROOM* user_room_open(WB_ROOM *next, char *uid, char *ticket,
                        MDF *sitenode, int usersn)
{
    if (!uid || !ticket || !sitenode) return NULL;

    WB_USER *user = _user_add(uid, ticket, sitenode, usersn);
    if (!user) return NULL;

    WB_ROOM *room = mos_calloc(1, sizeof(WB_ROOM));
    room->turncount = 0;
    room->usercount = 1;
    room->state = ROOM_STATE_INIT;
    mdf_init(&room->inode);
    room->user = user;
    user->room = room;

    room->next = next;

    return room;
}

bool user_room_add(WB_ROOM *room, char *uid, char *ticket,
                   MDF *sitenode, int usersn, int usernumber)
{
    if (!room || !uid || !ticket || !sitenode || !room->user) return false;

    WB_USER *user = _user_add(uid, ticket, sitenode, usersn);
    if (!user) return false;

    user->room = room;

    /* append user to room's last user */
    WB_USER *fuser = room->user;
    while (fuser->next) {
        fuser = fuser->next;
    }
    fuser->next = user;

    room->usercount++;

    if (room->usercount >= usernumber) room->state = ROOM_STATE_READY;

    return true;
}

void user_room_check(WB_ROOM *room, int efd)
{
    int fd;
    bool roomok;
    WB_USER *user;
    MDF *vnode;
    mdf_init(&vnode);

    while (room) {
        roomok = true;
        if (room->state == ROOM_STATE_READY ||
            room->state == ROOM_STATE_FAILURE) {

            mdf_clear(room->inode);

            user = room->user;
            while (user) {
                mtc_mt_foo("init user %d %s", user->usersn, user->uid);

                mdf_clear(user->callback);
                MDF *callback = mdf_get_nodef(user->sitenode, "callbacks.%d",
                                              user->usersn);
                mdf_copy(user->callback, NULL, callback , true);

                mdf_clear(vnode);
                if (!site_request(user->uid, user->ticket, user->sitenode,
                                  &fd, vnode)) {
                    mtc_mt_err("init urls request for %s failure", user->uid);
                    roomok = false;
                    break;
                }
                mdf_copy(room->inode, NULL, vnode, true);

                if (!site_room_init(fd, user->sitenode, room->inode, user->usersn)) {
                    mtc_mt_err("init room info %s failure", user->uid);
                    roomok = false;
                    break;
                }

                user->fd = fd;

                if (!poll_add(efd, user)) {
                    mtc_mt_err("add epoll fd failure");
                    roomok = false;
                    break;
                }

                user = user->next;
            }

            if (roomok) {
                char *roomnumber = mdf_get_value(room->inode, "roomnumber", NULL);
                room->turncount++;

                mtc_mt_foo("room %s 第 %d 轮游戏，房间号 %s",
                           room->user->uid, room->turncount, roomnumber);

                room->state = ROOM_STATE_RUNNING;
            } else {
                mtc_mt_err("room %s start failure", room->user->uid);

                room->state = ROOM_STATE_FAILURE;

                user = room->user;
                while (user) {
                    if (user->fd > 0) {
                        poll_del(efd, user->fd);
                        close(user->fd);
                        user->fd = -1;
                    }

                    user = user->next;
                }
            }
        } else if (room->state == ROOM_STATE_INIT) {
            mtc_mt_warn("room %s user not enough %d",
                        room->user->uid, room->usercount);
        } else if (room->state == ROOM_STATE_GAMEOVER) {
            room->usercount = 0;

            user = room->user;
            while (user) {
                room->usercount++;

                if (user->fd > 0) {
                    poll_del(efd, user->fd);
                    close(user->fd);
                    user->fd = -1;
                }

                user = user->next;
            }
            room->state = ROOM_STATE_READY;
        } else if (room->state == ROOM_STATE_CLOSED) {
            mtc_mt_warn("room %s closed by server", room->user->uid);

            user = room->user;
            while (user) {
                if (user->fd > 0) {
                    poll_del(efd, user->fd);
                    close(user->fd);
                    user->fd = -1;
                }

                user = user->next;
            }

            room->state = ROOM_STATE_READY;
        }

        room = room->next;
    }

    mdf_destroy(&vnode);
}

void user_room_destroy(WB_ROOM *room)
{
    while (room) {
        WB_ROOM *next = room->next;

        _user_destroy(room->user);
        mos_free(room);

        room = next;
    }
}
