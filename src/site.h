#ifndef __SITE_H__
#define __SITE_H__

bool site_request(char *uid, char *ticket, MDF *sitenode, MDF *snode, int *fd, MDF *vnode);

bool site_room_init(int fd, MDF *sitenode, MDF *roomnode, int usersn);

#endif
