#ifndef __POLL_H__
#define __POLL_H__

int  poll_create(WB_USER *wb_user);
int  poll_do(int efd, int *usercount);
void poll_destroy(int efd);

#endif
