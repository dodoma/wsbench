#include "reef.h"

#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "util.h"

char* app_ws_init_build(const char *req)
{
    unsigned char clientid[16];
    char sendid[25];
    for (int i = 0; i < 16; i++) {
        clientid[i] = mos_rand(127);
    }
    mb64_encode(clientid, 16, sendid, 25);

    MSTR str; mstr_init(&str);

    mstr_appendf(&str,
                 "GET %s HTTP/1.1\r\n"
                 "Sec-WebSocket-Version: 13\r\n"
                 "Sec-WebSocket-Key: %s\r\n"
                 "perMessageDeflate: false\r\n"
                 "Connection: Upgrade\r\n"
                 "Upgrade: websocket\r\n\r\n",
                 req, sendid);

    return str.buf;
}

char* app_http_get_build(const char *req)
{
    MSTR str; mstr_init(&str);

    mstr_appendf(&str, "GET %s HTTP/1.1\r\n\r\n", req);

    return str.buf;
}

char* app_http_post_build(const char *req, const char *payload)
{
    MSTR str; mstr_init(&str);

    mstr_appendf(&str, "POST %s HTTP/1.1\r\n"
                 "Content-Length: %zd\r\n\r\n"
                 "%s", req, strlen(payload), payload);

    return str.buf;
}

ssize_t app_http_send(int fd, const char *buf)
{
    if (fd <= 0 || !buf) return -1;

    size_t len = strlen(buf);

    MSG_DUMP("send:", buf, len);
    size_t c = 0;
    while (c < len) {
        int rv = send(fd, buf + c, len - c, MSG_NOSIGNAL);
        if (rv == len) return len;
        else if (rv < 0) return rv;
        else if (rv == 0) return c;

        c += rv;
    }

    return len;
}

ssize_t app_ws_send(int fd, const char *buf)
{
    if (fd <= 0 || !buf) return -1;

    size_t len = strlen(buf);
    unsigned char header[4];
    unsigned char *pos = header;
    int hbytes;

    if (len <= 125) {
        hbytes = 2;
        *pos = 0x81;
        pos++;
        *pos = 0x7F & len;
    } else {
        hbytes = 4;
        *pos = 0x81;
        pos++;
        *pos = 0x7F & 126;
        pos++;
        *(uint16_t*)pos = htons((uint16_t)len);
    }

    MSG_DUMP("send:", header, hbytes);
    send(fd, header, hbytes, MSG_NOSIGNAL);

    MSG_DUMP("send:", buf, len);
    size_t c = 0;
    while (c < len) {
        int rv = send(fd, buf + c, len - c, MSG_NOSIGNAL);
        if (rv == len) return len;
        else if (rv < 0) return rv;
        else if (rv == 0) return c;

        c += rv;
    }

    return len;
}

ssize_t app_recv(int fd, unsigned char *buf, size_t maxlen, char **stringp)
{
    if (fd <= 0 || !buf) return -1;

    memset(buf, 0x0, maxlen);

    int rv = recv(fd, buf, maxlen, 0);
    if (rv == 0) {
        mtc_mt_err("socket closed by server");
        return 0;
    } else if (rv < 0) {
        return rv;
    } else {
        MSG_DUMP("recv:", buf, rv);

        unsigned char *pos = buf + rv - 2;
        while (*pos != 0 && pos > buf) pos--;

        if (*pos == 0) *stringp = (char*)(pos + 1);
        else *stringp = (char*)buf;
    }

    return rv;
}
