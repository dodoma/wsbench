#include "reef.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

#include "url.h"
#include "app.h"

#define BUFFER_LEN 10240

static int _connect(MDF *snode)
{
    if (!snode) return -1;

    char *ip = mdf_get_value(snode, "ip", "127.0.0.1");
    int port = mdf_get_int_value(snode, "port", 0);
    int timeout = mdf_get_int_value(snode, "timeout", 0);

    mtc_mt_dbg("connect to %s %d %d", ip, port, timeout);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

    struct in_addr ia;
    int rv = inet_pton(AF_INET, ip, &ia);
    if (rv <= 0) {
        struct hostent *he = gethostbyname(ip);
        if (!he) return -1;
        ia.s_addr = *( (in_addr_t *) (he->h_addr_list[0]) );
    }

    struct sockaddr_in srvsa;
    srvsa.sin_family = AF_INET;
    srvsa.sin_port = htons(port);
    //srvsa.sin_addr.s_addr = inet_addr(ip);
    srvsa.sin_addr.s_addr = ia.s_addr;

    rv = connect(fd, (struct sockaddr *) &srvsa, sizeof(srvsa));
    if (rv < 0) return -1;

    return fd;
}

bool site_request(char *uid, char *ticket, MDF *sitenode, MDF *snode, int *fd, MDF *vnode)
{
    if (!uid || !sitenode) return false;

    int wsfd = 0;
    int lfd = 0;
    int retry = 0;

    /* TODO do this by js in config file */
    char randid[5];
    mstr_rand_string_fixlen(randid, 4);
    mdf_set_value(vnode, "__randid", randid);
    mdf_set_value(vnode, "_uuid", uid);
    mdf_set_value(vnode, "_ticket", ticket);
    mdf_set_int64_value(vnode, "_ts", time(NULL) * 1000);

    unsigned char recv_buf[BUFFER_LEN];

    MDF *cnode = mdf_get_child(sitenode, "urls");
    while (cnode) {
        char *method  = mdf_get_value(cnode, "method", NULL);
        char *req     = mdf_get_value(cnode, "req", NULL);
        char *resp    = mdf_get_value(cnode, "resp", NULL);
        char *payload = mdf_get_value(cnode, "payload", NULL);
        char *ps      = NULL;
        char *stringp = NULL;
        MDF *save_var = mdf_get_node(cnode, "save_var");

        if (!req) return false;
        req = url_var_replace(req, vnode);
        retry = 0;

        if (method && !strcmp(method, "ws init")) {
            if (wsfd <= 0) {
                wsfd = _connect(snode);
                if (wsfd <= 0) {
                    mtc_mt_err("connect to server falure");
                    return false;
                }
            }

            lfd = wsfd;
            ps = req;
            req = app_ws_init_build(req);
            mos_free(ps);

            app_http_send(lfd, req);
        } else if (!method || !strcmp(method, "ws")) {
            if (wsfd <= 0) {
                mtc_mt_err("can't request ws without ws init");
                return false;
            }

            lfd = wsfd;
            app_ws_send(lfd, req);
        } else {
            lfd = _connect(snode);
            if (lfd <= 0) {
                mtc_mt_err("connect to server failure");
                return false;
            }

            if (!strcmp(method, "get")) {
                ps = req;
                req = app_http_get_build(req);
                mos_free(ps);
            } else if (!strcmp(method, "post")) {
                ps = req;
                payload = url_var_replace(payload, vnode);
                req = app_http_post_build(req, payload);
                mos_free(payload);
                mos_free(ps);
            }

            app_http_send(lfd, req);
        }

        if (resp) {
        retry:
            memset(recv_buf, 0x0, BUFFER_LEN);
            int len = app_recv(lfd, recv_buf, BUFFER_LEN, &stringp);
            if (len == 0) {
                mtc_mt_err("server closed connect");
                return false;
            } else if (len < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
                mtc_mt_err("unknown error on receive");
                return false;
            }

            if (!url_var_save(vnode, resp, save_var, stringp)) {
                if (retry++ <= 2) goto retry;

                mtc_mt_err("expect response failure %s %s", recv_buf, resp);
                return false;
            }
        }

        if (lfd != wsfd) close(lfd);

        mos_free(req);

        cnode = mdf_node_next(cnode);
    }

    if (wsfd <= 0) {
        mtc_mt_err("no websocket connect established");
        return false;
    }

    *fd = wsfd;

    return true;
}

bool site_room_init(int fd, MDF *sitenode, MDF *roomnode, int usersn)
{
    if (fd < 0 || !sitenode || !roomnode) return false;

    int retry;

    unsigned char recv_buf[BUFFER_LEN];

    MDF *cnode = mdf_get_childf(sitenode, "room.actions.%d", usersn);
    while (cnode) {
        char *req     = mdf_get_value(cnode, "req", NULL);
        char *resp    = mdf_get_value(cnode, "resp", NULL);
        char *stringp = NULL;
        MDF *save_var = mdf_get_node(cnode, "save_var");

        if (!req) return false;
        req = url_var_replace(req, roomnode);
        retry = 0;

        app_ws_send(fd, req);

        if (resp) {
        retry:
            memset(recv_buf, 0x0, BUFFER_LEN);
            int len = app_recv(fd, recv_buf, BUFFER_LEN, &stringp);
            if (len == 0) {
                mtc_mt_err("server closed connect");
                return false;
            } else if (len < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
                mtc_mt_err("unknown error on receive");
                return false;
            }

            if (!url_var_save(roomnode, resp, save_var, stringp)) {
                if (retry++ <= 2) goto retry;

                mtc_mt_err("expect response failure %s %s", recv_buf, resp);
                return false;
            }
        }

        mos_free(req);

        cnode = mdf_node_next(cnode);
    }

    return true;
}
