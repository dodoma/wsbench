#ifndef __NET_H__
#define __NET_H__

#ifdef DEBUG_MSG
#define MSG_DUMP(pre, p, psize)                                     \
    do {                                                            \
        if ((ssize_t)(psize) > 0) {                                 \
            char zstra[psize*2+1];                                  \
            mstr_bin2str((uint8_t*)p, psize, zstra);                \
            mtc_mt_dbg("%s%zu %s", pre, (size_t)(psize), zstra);    \
        }                                                           \
    } while (0)
#else
#define MSG_DUMP(pre, p, psize)
#endif

ssize_t net_send(int fd, unsigned char *buf, size_t len);
ssize_t net_recv(int fd, unsigned char *buf, size_t maxlen);

#endif
