

#include "reef.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#include "url.h"
#include "net.h"

int site_connect(MDF *snode)
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
    //setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    //setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

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

bool site_request(int fd, char *uid, char *ticket, MDF *sitenode, MDF *snode)
{
#define BUFFER_LEN 10240
    if (fd <= 0 || !uid || !sitenode) return false;

    MDF *vnode;
    mdf_init(&vnode);

    int wsfd = 0;

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
        char *ip = mdf_get_value(snode, "ip", "127.0.0.1");
        int port = mdf_get_int_value(snode, "port", 0);

        char *req     = mdf_get_value(cnode, "req", NULL);
        char *payload = mdf_get_value(cnode, "payload", NULL);
        char *prot    = mdf_get_value(cnode, "protocol", NULL);
        char *resp    = mdf_get_value(cnode, "resp", NULL);
        MDF *save_var = mdf_get_node(cnode, "save_var");

        char *ps = NULL;

        int lfd;

        if (!req) goto nexturl;

        req = url_var_replace(req, vnode);
        if (prot && !strcmp(prot, "http")) {
            lfd = fd;
            ps = req;
            req = url_http_build(ip, port, req);
            mos_free(ps);
        } else if (prot && !strcmp(prot, "post")) {
            lfd = fd;
            ps = req;
            payload = url_var_replace(payload, vnode);
            req = url_http_post_build(ip, port, req, payload);
            mos_free(payload);
            mos_free(ps);
        } else if (prot && !strcmp(prot, "ws")) {
            wsfd = site_connect(snode);
            lfd = wsfd;

            ps = req;
            req = url_ws_build(ip, port, req);
            mos_free(ps);
        } else {
            lfd = wsfd;
            unsigned char header[4];
            unsigned char *pos = header;
            int hbytes;

            size_t slen = strlen(req);
            if (slen <= 125) {
                hbytes = 2;
                *pos = 0x81;
                pos++;
                *pos = 0x7F & slen;
            } else {
                hbytes = 4;
                *pos = 0x81;
                pos++;
                *pos = 0x7F & 126;
                pos++;
                *(uint16_t*)pos = htons((uint16_t)slen);
            }

            net_send(lfd, header, hbytes);
        }

        mtc_mt_dbg("request %s", req);

        net_send(lfd, (unsigned char*)req, 0);

        if (resp) {
            memset(recv_buf, 0x0, BUFFER_LEN);
            int len = net_recv(lfd, recv_buf, BUFFER_LEN);
            if (len > 0) {
                mtc_mt_dbg("receive %d %s", len, (char*)recv_buf);
            } else {
                mtc_mt_err("receive error %d", len);
                break;
            }

            if (!url_var_save(vnode, resp, save_var, (char*)recv_buf)) {
                mtc_mt_err("save url var failure %s %s", recv_buf, resp);
                break;
            }
        }

        mos_free(req);

    nexturl:
        cnode = mdf_node_next(cnode);
    }

    mdf_destroy(&vnode);
    return true;
}
