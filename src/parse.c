#include "reef.h"

#include <arpa/inet.h>

#include "parse.h"
#include "url.h"
#include "app.h"

#define TCP_PACKET_MAX_LEN 1048576

enum {
    OP_STREAM = 0,
    OP_TEXT,
    OP_BINARY,
    OP_CLOSE = 8,
    OP_PING,
    OP_PONG
};

static void _parse_text_event(WB_USER *user)
{
    MDF *cnode = mdf_get_child(user->callback, NULL);
    while (cnode) {
        char *onstring = mdf_get_value(cnode, "on", NULL);
        char *req      = mdf_get_value(cnode, "req", NULL);
        int count_set  = mdf_get_int_value(cnode, "count", 1);
        int count_run  = mdf_get_int_value(cnode, "count_run", 0);
        bool proceed   = mdf_get_bool_value(cnode, "old", false);
        bool repeat    = mdf_get_bool_value(cnode, "repeat", false);
        int delay      = mdf_get_int_value(cnode, "delay", 0);
        bool over      = mdf_get_bool_value(cnode, "over", false);
        MDF *save_var  = mdf_get_node(cnode, "save_var");

        if (url_var_save(user->inode, onstring, save_var, (char*)user->payload)) {
            mtc_mt_dbg("%s on message %s", user->uid, onstring);

            if (proceed && !repeat && (req || !over)) {
                mtc_mt_dbg("proceeded");
                break;
            }

            /* message comed in */
            count_run++;

            if (count_set > count_run) mdf_set_int_value(cnode, "count_run", count_run);
            else {
                if (req) {
                    req = url_var_replace(req, user->room->inode, user->inode);
                    if (delay == 0) {
                        app_ws_send(user->fd, req);
                        mos_free(req);
                    } else {
                        /* req async */
                        user_message_append(user, req, delay);
                    }
                }

                if (over == true) {
                    user->room->usercount--;
                    mtc_mt_dbg("game over for user %s remain user: %d", user->uid, user->room->usercount);
                    if (user->room->usercount <= 0) {
                        user->room->state = ROOM_STATE_GAMEOVER;
                    }
                }

                mdf_set_int_value(cnode, "count_run", 0);
                mdf_set_bool_value(cnode, "old", true);
            }

            break;
        }

        cnode = mdf_node_next(cnode);
    }

    if (!cnode) mtc_mt_warn("%s unknown message %s", user->uid, user->payload);
}

static void _parse_packet(WB_USER *user, bool finish)
{
    unsigned char ptmp;

    mtc_mt_noise("parse packet %d with payload length "FMT_UINT64"", user->opcode, user->payloadsize);

    switch (user->opcode) {
    case OP_TEXT:
        ptmp = user->payload[user->payloadsize];
        user->payload[user->payloadsize] = '\0';

        mtc_mt_noise("text: %s", user->payload);

        _parse_text_event(user);

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
}

void parse_buf(WB_USER *user, unsigned char *buf, size_t len)
{
    uint32_t totaltoget = 0;

    if (!user || !buf) return;

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
            user->rcvbuf = mos_calloc(1, TCP_PACKET_MAX_LEN);
            memcpy(user->rcvbuf, buf, len);
        }
        user->rcvlen = len;
        return;
    }

    if (totaltoget < len) {
        user->excess = len - totaltoget;
        len = totaltoget;
    }

    _parse_packet(user, finish);

    if (user->excess) {
        memmove(buf, buf + len, user->excess);
        user->rcvlen = user->excess;
        user->excess = 0;

        parse_buf(user, buf, user->rcvlen);
        return;
    }

    mos_free(user->rcvbuf);
    user->rcvbuf = NULL;
    user->rcvlen = 0;
    user->excess = 0;

    user->payload = NULL;
    user->payloadsize = 0;

    return;

error_exit:
    close(user->fd);
    return;
}
