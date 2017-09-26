#include "reef.h"

char* url_http_build(const char *host, int port, const char *url)
{
    if (port == 0) port = 80;

    MSTR str;

    mstr_init(&str);

    mstr_appendf(&str, "GET %s HTTP/1.1\r\n\r\n", url);

    return str.buf;
}

char* url_http_post_build(const char *host, int port, const char *url, const char *payload)
{
    if (port == 0) port = 80;

    MSTR str;

    mstr_init(&str);

    mtc_mt_dbg("post %s", url);
    mtc_mt_dbg("post %s", payload);

    mstr_appendf(&str, "POST %s HTTP/1.1\r\n"
                 "Content-Length: 177\r\n\r\n"
                 "%s",
                 url, payload);

    return str.buf;
}

char* url_ws_build(const char *host, int port, const char *url)
{
    if (port == 0) port = 443;
    MSTR str;

    mstr_init(&str);

    unsigned char clientid[16];
    char sendid[25];

    for (int i = 0; i < 16; i++) {
        clientid[i] = mos_rand(127);
    }
    mb64_encode(clientid, 16, sendid, 25);

#if 0
    return
        "GET / HTTP/1.1\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: WybDdhLLaKbABqrZGMdw3g==\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
        "Host: echo.websocket.org\r\n\r\n";
#else
    mstr_appendf(&str,
                 "GET %s HTTP/1.1\r\n"
                 "Sec-WebSocket-Version: 13\r\n"
                 "Sec-WebSocket-Key: %s\r\n"
                 "Connection: Upgrade\r\n"
                 "Upgrade: websocket\r\n"
                 "Host: echo.websocket.org\r\n"
                 "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n\r\n", url, sendid);

    return str.buf;

#endif

}

char* url_var_replace(const char *src, MDF *vnode)
{
    MERR *err;

    MSTR str;
    mstr_init(&str);

    /* TODO move out for better perform */
    MRE *reo = mre_init();

    err = mre_compile(reo, "\\$\\{([_a-zA-Z]+)\\}");
    RETURN_V_NOK(err, NULL);

    const char *pstart, *sp, *ep;
    pstart = sp = ep = src;

    char key[1024];

    uint32_t count = mre_match_all(reo, src, false);
    for (int i = 0; i < count; i++) {
        if (mre_sub_get(reo, i, 1, &sp, &ep)) {
            mstr_appendn(&str, pstart, (sp - pstart - 2));
            pstart = ep + 1;

            snprintf(key, sizeof(key), "%.*s", (int)(ep - sp), sp);
            mstr_append(&str, mdf_get_value_stringfy(vnode, key, NULL));
        } else break;
    }

    mstr_append(&str, pstart);

    mre_destroy(&reo);

    return str.buf;
}

bool url_var_save(MDF *vnode, const char *resp, MDF *save_var, const char *recv_buf)
{
    MERR *err;

    if (!resp) return true;
    if (!save_var || mdf_child_count(save_var, NULL) <= 0) return true;

    if (!vnode || !recv_buf) return false;

    MRE *reo = mre_init();

    err = mre_compile(reo, resp);
    RETURN_V_NOK(err, false);

    const char *sp, *ep;
    sp = ep = NULL;

    if (!mre_match(reo, recv_buf, false)) {
        mtc_mt_err("match %s failure", recv_buf);
        return false;
    }

    int sn = 1;
    MDF *cnode = mdf_get_child(save_var, NULL);
    while (cnode) {
        char *key = mdf_get_value(cnode, NULL, NULL);

        if (mre_sub_get(reo, 0, sn++, &sp, &ep)) {
            mdf_set_valuef(vnode, "%s=%.*s", key, (int)(ep - sp), sp);
        } else {
            mtc_mt_err("get resp for %d %s failure", sn, key);
            return false;
        }

        cnode = mdf_node_next(cnode);
    }

    mre_destroy(&reo);

    return true;
}
