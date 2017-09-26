#ifndef __SITE_H__
#define __SITE_H__

int  site_connect(MDF *snode);
bool site_request(int fd, char *uid, char *ticket, MDF *sitenode, MDF *snode);

#endif
