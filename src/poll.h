#ifndef __POLL_H__
#define __POLL_H__

struct epoll_event* poll_create(int *fd);
bool poll_add(int efd, WB_USER *user);
void poll_del(int efd, int fd);
void poll_do(int efd, struct epoll_event *events,
             unsigned char *recv_buf, int maxlen);
void poll_destroy(int efd, struct epoll_event *events);

#endif
