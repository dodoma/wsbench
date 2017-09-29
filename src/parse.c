#include "reef.h"

#include <arpa/inet.h>

#include "parse.h"
#include "url.h"
#include "app.h"

#define TCP_PACKET_MAX_LEN 104857600

enum {
    OP_STREAM = 0,
    OP_TEXT,
    OP_BINARY,
    OP_CLOSE = 8,
    OP_PING,
    OP_PONG
};

static int _parse_text_event(WB_USER *user)
{
    MERR *err;

    MRE *reo = mre_init();

    MDF *cnode = mdf_get_child(user->callback, NULL);
    while (cnode) {
        char *onstring = mdf_get_value(cnode, "on", NULL);
        char *req = mdf_get_value(cnode, "req", NULL);
        int count = mdf_get_int_value(cnode, "count", 1);

        err = mre_compile(reo, onstring);
        TRACE_NOK_MT(err);

        if (mre_match(reo, (char*)user->payload, false)) {
            mtc_mt_dbg("%s on message %s", user->uid, onstring);

            if (mdf_get_bool_value(cnode, "old", false)) {
                mtc_mt_dbg("proceeded");
                break;
            }

            if (count > 1) mdf_add_int_value(cnode, "count", -1);
            else {
                if (req) {
                    req = url_var_replace(req, user->roomnode);
                    app_ws_send(user->fd, req);
                    mos_free(req);

                    mdf_set_bool_value(cnode, "old", true);

                    if (mdf_get_bool_value(cnode, "over", false) == true) {
                        mre_destroy(&reo);
                        return 0;
                    }
                }
            }

            break;
        }

        cnode = mdf_node_next(cnode);
    }

    mre_destroy(&reo);

    if (!cnode) mtc_mt_warn("%s unknown message %s", user->uid, user->payload);

    return 1;
}

static int _parse_packet(WB_USER *user, bool finish)
{
    unsigned char ptmp;

    mtc_mt_noise("parse packet %d with payload length "FMT_UINT64"", user->opcode, user->payloadsize);

    switch (user->opcode) {
    case OP_TEXT:
        ptmp = user->payload[user->payloadsize];
        user->payload[user->payloadsize] = '\0';

        if (_parse_text_event(user) == 0) return 0;

        mtc_mt_noise("text: %s", user->payload);

        user->payload[user->payloadsize] = ptmp;
        break;
    case OP_PING:
        break;
    case OP_PONG:
        break;
    case OP_CLOSE:
        close(user->fd);
        break;
    case OP_STREAM:
    case OP_BINARY:
        mtc_mt_dbg("%s "FMT_UINT64" data received", finish ? "finished" : "unfinish", user->payloadsize);
        break;

    default:
        mtc_mt_err("opcode not support yet");
    }

    return 1;
}

int parse_buf(WB_USER *user, unsigned char *buf, size_t len)
{
    uint32_t totaltoget = 0;

    if (!user || !buf) return -1;

    unsigned char *pos = buf;
    totaltoget = 1;
    bool finish = true;

    /* FIN, RSV1,2,3, opcode */
    if (*pos >> 7 != 1) finish = false;
    user->opcode = *pos & 0x0F;

    if (!finish && (user->opcode != OP_BINARY && user->opcode != OP_STREAM)) {
        mtc_mt_err("text continus packet not support yet");
        goto error_exit;
    }

    if ((user->opcode < OP_CLOSE) && len > 1) {
        /* mask, payload len */
        pos += 1;
        if (*pos >> 7) {
            mtc_mt_err("masked return packet not support yet");
            goto error_exit;
        }
        user->payloadsize = (*pos) & 0x7F;

        pos += 1;
        totaltoget += 1;
        if (user->payloadsize == 126) {
            user->payloadsize = *(uint16_t*)pos;
            user->payloadsize = ntohs(user->payloadsize);
            pos += 2;
            totaltoget += 2;
        } else if (user->payloadsize == 127) {
            user->payloadsize = *(uint64_t*)pos;
            user->payloadsize = ntohll(user->payloadsize);
            pos += 8;
            totaltoget += 8;
        }

        user->payload = pos;
        totaltoget += user->payloadsize;
        if (totaltoget > TCP_PACKET_MAX_LEN) {
            mtc_mt_err("llong packet not support yet");
            goto error_exit;
        }
    }

    if (totaltoget > len) {
        if (user->rcvbuf == NULL) {
            user->rcvbuf = mos_malloc(TCP_PACKET_MAX_LEN);
            memcpy(user->rcvbuf, buf, len);
        }
        user->rcvlen = len;
        return -1;
    }

    if (totaltoget < len) {
        user->excess = len - totaltoget;
        len = totaltoget;
    }

    if (_parse_packet(user, finish) == 0) return 0;

    if (user->excess) {
        memmove(buf, buf + len, user->excess);
        user->rcvlen = user->excess;
        user->excess = 0;

        return parse_buf(user, buf, user->rcvlen);
    }

    mos_free(user->rcvbuf);
    user->rcvlen = 0;
    user->excess = 0;

    user->payload = NULL;
    user->payloadsize = 0;

    return 1;

error_exit:
    close(user->fd);
    return 0;
}
