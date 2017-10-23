#ifndef __APP_H__
#define __APP_H__

char* app_ws_init_build(const char *req);
char* app_http_get_build(const char *req);
char* app_http_post_build(const char *req, const char *payload);

ssize_t app_http_send(int fd, const char *buf);
ssize_t app_ws_send(int fd, const char *buf);
ssize_t app_recv(int fd, unsigned char *buf, size_t maxlen);

#endif
