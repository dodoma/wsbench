#ifndef __URL_H__
#define __URL_H__

char* url_var_replace(const char *src, MDF *vnode);
bool  url_var_save(MDF *vnode, const char *resp, MDF *save_var, const char *recv_buf);

#endif
