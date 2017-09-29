#include "reef.h"

#include "util.h"
#include "site.h"
#include "user.h"
#include "poll.h"
#include "app.h"

#define MAX_THREAD_NUM 1

#define DEFAULT_CONFIGFILE "./config.json"

MDF  *g_cfg = NULL;
char *g_config_filename = NULL;

static bool dad_call_me_back = false;

time_t g_ctime = 0;

static void* _bench_do(void *arg)
{
    MERR *err;

    int threadsn = *(int*)arg;

    char fname[PATH_MAX];
    snprintf(fname, sizeof(fname), "logs/%04d.log", threadsn);
    mtc_mt_init("-", MTC_DEBUG);
    mtc_mt_dbg("I am bencher %d", threadsn);

    WB_USER *wb_user;
    MDF *cnode, *sitenode;
    int usercount;
    int turncount = 0;

replay:
    wb_user = NULL;
    cnode = mdf_get_child(g_cfg, "sites");
    sitenode = NULL;
    mdf_init(&sitenode);
    usercount = 0;

    turncount++;

    /*
     * 1. every site
     */
    while (cnode) {
        char *sitename = mdf_get_value(cnode, "name", "unknown");

        mtc_mt_dbg("init site %s", sitename);

        mdf_clear(sitenode);
        err = mdf_json_import_filef(sitenode, "%s.json", sitename);
        RETURN_V_NOK(err, NULL);

        int usernumber = mdf_get_int_value(sitenode, "room.usernumber", 4);
        MDF *callbacknode = mdf_get_node(sitenode, "callbacks");
        MDF *roomnode = NULL;
        MDF *vnode;

        mdf_init(&vnode);
        mdf_init(&roomnode);

        /*
         * 2. every user
         */
        int usersn = 0;
        MDF *unode = mdf_get_nodef(sitenode, "users[%d][0]", threadsn);
        while (unode) {
            char *uid = mdf_get_value(unode, "uuid", NULL);
            char *ticket = mdf_get_value(unode, "ticket", NULL);
            usersn = usersn % usernumber;

            mtc_mt_foo("init user %d %s", usersn, uid);

            int fd = 0;
            mdf_clear(vnode);
            /* a. urls request and response */
            if (!site_request(uid, ticket, sitenode, cnode, &fd, vnode)) {
                mtc_mt_err("init urls request for %s failure", uid);
                break;
            }

            /* b. room init */
            if (usersn == 0) mdf_clear(roomnode);
            mdf_copy(roomnode, NULL, vnode, true);
            if (!site_room_init(fd, sitenode, roomnode, usersn)) {
                mtc_mt_err("init room master info failure");
                break;
            }
            if (usersn == 0)
                mtc_mt_foo("room number %s", mdf_get_value(roomnode, "roomnumber", NULL));

            /* c. regist callback */
            wb_user = user_add(wb_user, fd, uid, ticket,
                               roomnode, callbacknode, usersn);
            if (!wb_user) {
                mtc_mt_err("user add failure.");
                break;
            }

            usersn++;
            usercount++;
            unode = mdf_node_next(unode);
        }

        mdf_destroy(&vnode);
        mdf_destroy(&roomnode);

        cnode = mdf_node_next(cnode);
    }

    mdf_destroy(&sitenode);

    /*
     * 3. poll websocket fds
     */
    int efd = poll_create(wb_user);
    if (efd <= 0) {
        mtc_mt_err("create poll fd failure");
        return NULL;
    }

    time_t last_heartbeat = g_ctime;
    while (!dad_call_me_back) {

        if (poll_do(efd, &usercount) == 0) {
            mtc_mt_foo("第 %d 轮游戏结束, 重新开始", turncount);
            poll_destroy(efd);
            user_destroy(wb_user);
            goto replay;
        }

        if (g_ctime > (last_heartbeat + 25)) {
            last_heartbeat = g_ctime;

            WB_USER *user = wb_user;
            while (user) {
                app_ws_send(user->fd, "2");
                user = user->next;
            }
        }
    }

    poll_destroy(efd);
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

    g_ctime = time(NULL);

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
        g_ctime = time(NULL);
    }

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_join(*threads[i], NULL);
        mos_free(threads[i]);
    }

    return 0;
}
