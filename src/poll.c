#include "reef.h"

#include <sys/epoll.h>
#include <errno.h>

#include "user.h"
#include "app.h"
#include "parse.h"

#define MAXEVENTS  1024

struct epoll_event* poll_create(int *fd)
{
    int efd = epoll_create1(0);
    if (efd < 0) {
        mtc_mt_err("create epoll fd failure");
        return NULL;
    }

    struct epoll_event *events = mos_calloc(MAXEVENTS, sizeof(struct epoll_event));

    *fd = efd;

    return events;
}

bool poll_add(int efd, WB_USER *user)
{
    if (efd <= 0 || !user || user->fd <= 0) return false;

    struct epoll_event event;
    // event.data.fd = user->fd;    // event.data 是个 union，此处用 ptr
    event.data.ptr = user;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, user->fd, &event) < 0) return false;

    return true;
}

void poll_del(int efd, int fd)
{
    if (efd <= 0 || fd <= 0) return;

    epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
}

void poll_do(int efd, struct epoll_event *events,
             unsigned char *recv_buf, int maxlen)
{
    char *stringp = NULL;

    int n = epoll_wait(efd, events, MAXEVENTS, 2000);
    for (int i = 0; i < n; i++) {
        WB_USER *user = (WB_USER*)events[i].data.ptr;
        int fd = user->fd;

        mtc_mt_noise("poll return on %d %s %d", i, user->uid, user->fd);

        if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
            (!(events[i].events & EPOLLIN))) {
            mtc_mt_err("epoll %s error\n", user->uid);
            poll_del(efd, fd);
            close(fd);
            user->room->state = ROOM_STATE_CLOSED;
            continue;
        }

        memset(recv_buf, 0x0, maxlen);
        int len = app_recv(fd, recv_buf, maxlen, &stringp);
        if (len == 0) {
            mtc_mt_err("server closed connect");
            poll_del(efd, fd);
            close(fd);
            user->room->state = ROOM_STATE_CLOSED;
            continue;
        } else if (len < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
            mtc_mt_err("unknown error on receive %s %d %d %s",
                       user->uid, fd, len, strerror(errno));
            continue;
        }

        if (user->rcvbuf == NULL) {
            parse_buf(user, recv_buf, len);
        } else {
            memcpy(user->rcvbuf + user->rcvlen, recv_buf, len);
            user->rcvbuf += len;
            parse_buf(user, user->rcvbuf, user->rcvlen);
        }
    }
}

void poll_destroy(int efd, struct epoll_event *events)
{
    mos_free(events);
    close(efd);
}
