#include "reef.h"

#include <sys/epoll.h>
#include <errno.h>

#include "user.h"
#include "app.h"
#include "parse.h"

#define BUFFER_LEN 10240
#define MAXEVENTS 64

int poll_create(WB_USER *wb_user)
{
    int efd = epoll_create1(0);
    if (efd < 0) {
        mtc_mt_err("create epoll fd failure");
        return 0;
    }

    WB_USER *user = wb_user;
    while (user) {
        struct epoll_event event;
        event.data.fd = user->fd;
        event.data.ptr = user;
        event.events = EPOLLIN | EPOLLET;

        int rv = epoll_ctl(efd, EPOLL_CTL_ADD, user->fd, &event);
        if (rv < 0) {
            mtc_mt_err("add epoll fd failure");
            return 0;
        }

        user = user->next;
    }

    return efd;
}

int poll_do(int efd, int *usercount)
{
    /* TODO move calloc out for better performence */
    struct epoll_event *events = mos_calloc(MAXEVENTS,
                                            sizeof(struct epoll_event));

    unsigned char recv_buf[BUFFER_LEN];
    char *stringp = NULL;

    int n = epoll_wait(efd, events, MAXEVENTS, 2000);
    for (int i = 0; i < n; i++) {
        WB_USER *user = (WB_USER*)events[i].data.ptr;
        int fd = user->fd;

        mtc_mt_noise("poll return on %d %s %d", i, user->uid, user->fd);

        if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
            (!(events[i].events & EPOLLIN))) {
            mtc_mt_err("epoll %s error\n", user->uid);
            close(fd);
            continue;
        }

        memset(recv_buf, 0x0, BUFFER_LEN);
        int len = app_recv(fd, recv_buf, BUFFER_LEN, &stringp);
        if (len == 0) {
            mtc_mt_err("server closed connect");
            continue;
        } else if (len < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
            mtc_mt_err("unknown error on receive %s %d %d %s",
                       user->uid, fd, len, strerror(errno));
            continue;
        }

        if (user->rcvbuf == NULL) {
            if (parse_buf(user, recv_buf, len) == 0) *usercount = *usercount - 1;
        } else {
            memcpy(user->rcvbuf + user->rcvlen, recv_buf, len);
            user->rcvbuf += len;
            if (parse_buf(user, user->rcvbuf, user->rcvlen) == 0) *usercount = *usercount - 1;
        }
    }

    mos_free(events);

    if (*usercount <= 0) return 0;

    return 1;
}

void poll_destroy(int efd)
{
    close(efd);
}
