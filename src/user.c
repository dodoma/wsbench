#include "reef.h"

#include "user.h"

WB_USER* user_add(WB_USER *next, int fd, char *uid, MDF *callback)
{
    if (fd <= 0 || !uid || !callback) return NULL;

    WB_USER *user = mos_calloc(1, sizeof(WB_USER));

    mdf_init(&user->callback);

    user->fd = fd;
    user->uid = strdup(uid);
    mdf_copy(user->callback, NULL, callback, true);
    user->next = next;

    return user;
}

void user_destroy(WB_USER *user)
{
    while (user) {
        WB_USER *next = user->next;

        mos_free(user->uid);
        mdf_destroy(&user->callback);
        close(user->fd);
        mos_free(user);

        user = next;
    }
}
