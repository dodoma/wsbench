#include "reef.h"

#include "util.h"
#include "site.h"
#include "user.h"
#include "poll.h"

#define MAX_THREAD_NUM 1

#define DEFAULT_CONFIGFILE "./config.json"

MDF  *g_cfg = NULL;
char *g_config_filename = NULL;

static bool dad_call_me_back = false;

static void* _bench_do(void *arg)
{
    MERR *err;

    int threadsn = *(int*)arg;

    char fname[PATH_MAX];
    snprintf(fname, sizeof(fname), "logs/%04d.log", threadsn);
    mtc_mt_init("-", MTC_DEBUG);
    mtc_mt_dbg("I am bencher %d", threadsn);

    WB_USER *wb_user = NULL;

    MDF *cnode = mdf_get_child(g_cfg, "sites");
    MDF *sitenode = NULL;
    mdf_init(&sitenode);

    /*
     * 1. every site
     */
    while (cnode) {
        char *sitename = mdf_get_value(cnode, "name", "unknown");

        mtc_mt_dbg("init site %s", sitename);

        mdf_clear(sitenode);
        err = mdf_json_import_filef(sitenode, "%s.json", sitename);
        RETURN_V_NOK(err, NULL);

        /*
         * 2. every user
         */
        MDF *unode = mdf_get_nodef(sitenode, "users[%d][0]", threadsn);
        while (unode) {
            char *uid = mdf_get_value(unode, "uuid", NULL);
            char *ticket = mdf_get_value(unode, "ticket", NULL);

            mtc_mt_dbg("init user %s", uid);

            /* a. connect */
            int fd = site_connect(cnode);
            if (fd <= 0) {
                mtc_mt_err("connect to site %s failure.", sitename);
                break;
            }

            /* b. urls request and response */
            if (!site_request(fd, uid, ticket, sitenode, cnode)) {
                mtc_mt_err("urls request and response failure");
                break;
            }

            /* c. regist callback */
            wb_user = user_add(wb_user, fd, uid, mdf_get_node(sitenode, "callbacks"));
            if (!wb_user) {
                mtc_mt_err("user add failure.");
                break;
            }

            unode = mdf_node_next(unode);
        }

        cnode = mdf_node_next(cnode);
    }

    mdf_destroy(&sitenode);


    while (!dad_call_me_back) {
        /*
         * 4. poll fds
         */
        //poll_do(wb_user);
        sleep(1);
    }

    user_destroy(wb_user);

    return NULL;
}

int main(int argc, char *argv[])
{
    MERR *err;

    mdf_init(&g_cfg);
    g_config_filename = argv[1] ? argv[1] : DEFAULT_CONFIGFILE;

    err = mdf_json_import_file(g_cfg, g_config_filename);
    DIE_NOK(err);

    mtc_init("-", MTC_DEBUG);

    set_conio_terminal_mode();

    pthread_t *threads[MAX_THREAD_NUM];
    int num[MAX_THREAD_NUM];
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        threads[i] = mos_calloc(1, sizeof(pthread_t));
        num[i] = i;
    }

    dad_call_me_back = false;

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_create(threads[i], NULL, _bench_do, (void*)&num[i]);
    }

    printf("wsbench running. 'q' to exit.\n");

    while (1) {
        if (kbdhit()) {
            char c = getchar();

            if (c == 'q') {
                dad_call_me_back = true;
                break;
            }
        }
    }

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_join(*threads[i], NULL);
        mos_free(threads[i]);
    }

    return 0;
}
