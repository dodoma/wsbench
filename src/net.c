#include "reef.h"

#include <errno.h>
#include <sys/socket.h>

#include "net.h"

ssize_t net_send(int fd, unsigned char *buf, size_t len)
{
    if (fd <= 0 || !buf) return -1;

    if (len == 0) len = strlen((char*)buf);

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

ssize_t net_recv(int fd, unsigned char *buf, size_t maxlen)
{
    if (fd <= 0 || !buf) return -1;

    sleep(2);

    memset(buf, 0x0, maxlen);

    mtc_mt_dbg("receive from %d", fd);
    int rv = recv(fd, buf, maxlen, 0);
    if (rv == 0) {
        mtc_mt_err("socket closed by server");
        return 0;
    } else if (rv < 0) {
        return rv;
    } else {
        MSG_DUMP("recv:", buf, rv);
        unsigned char *pos = buf;
        while (*pos != '\0' && (pos - buf) <  (rv - 2)) pos ++;
        if (*pos == 0) memcpy(buf, pos + 1, rv - (pos - buf));
    }

    return rv;
}
