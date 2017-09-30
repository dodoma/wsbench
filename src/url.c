#include "reef.h"

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
            if (mdf_path_exist(vnode, key)) {
                char *value = mdf_get_value_stringfy(vnode, key, NULL);
                mstr_append(&str, value);
                mos_free(value);
            } else {
                mstr_append(&str, "${");
                mstr_appendn(&str, sp, (ep - sp));
                mstr_append(&str, "}");
            }

        } else break;
    }

    mstr_append(&str, pstart);

    mre_destroy(&reo);

    return str.buf;
}

bool url_var_save(MDF *vnode, const char *resp, MDF *save_var, const char *recv_buf)
{
#define RETURN(ret)                             \
    do {                                        \
        mre_destroy(&reo);                      \
        return (ret);                           \
    } while (0)

    MERR *err;

    if (!resp) return true;

    if (!vnode || !recv_buf) return false;

    MRE *reo = mre_init();

    err = mre_compile(reo, resp);
    RETURN_V_NOK(err, false);

    const char *sp, *ep;
    sp = ep = NULL;

    //mtc_mt_dbg("xxxx %s", recv_buf);

    if (!mre_match(reo, recv_buf, false)) {
        mtc_mt_warn("match %s %s failure", resp, recv_buf);
        RETURN(false);
    }

    int sn = 1;
    MDF *cnode = mdf_get_child(save_var, NULL);
    while (cnode) {
        char *key = mdf_get_value(cnode, NULL, NULL);

        if (mre_sub_get(reo, 0, sn++, &sp, &ep)) {
            mdf_set_valuef(vnode, "%s=%.*s", key, (int)(ep - sp), sp);
        } else {
            mtc_mt_err("get resp for %d %s failure", sn, key);
            RETURN(false);
        }

        cnode = mdf_node_next(cnode);
    }

    RETURN(true);
}
