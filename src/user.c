#include "reef.h"

#include "user.h"

WB_USER* user_add(WB_USER *next, int fd, char *uid, char *ticket,
                  MDF *roomnode, MDF *callback, int usersn)
{
    if (fd <= 0 || !uid || !ticket || !callback) return NULL;

    WB_USER *user = mos_calloc(1, sizeof(WB_USER));

    mdf_init(&user->roomnode);
    mdf_init(&user->callback);

    user->fd = fd;
    user->uid = strdup(uid);
    user->ticket = strdup(ticket);
    mdf_copy(user->roomnode, NULL, roomnode, true);
    mdf_copy(user->callback, NULL, mdf_get_nodef(callback, "%d", usersn) , true);

    user->rcvbuf = NULL;
    user->rcvlen = 0;
    user->excess = 0;
    user->opcode = 0;
    user->payload = 0;
    user->payloadsize = 0;

    user->next = next;

    return user;
}

void user_destroy(WB_USER *user)
{
    while (user) {
        WB_USER *next = user->next;

        mos_free(user->uid);
        mos_free(user->ticket);
        mdf_destroy(&user->roomnode);
        mdf_destroy(&user->callback);
        mos_free(user->rcvbuf);

        close(user->fd);
        mos_free(user);

        user = next;
    }
}
