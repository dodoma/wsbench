#include "reef.h"

#include "config.h"
#include "util.h"
#include "site.h"
#include "user.h"
#include "poll.h"

MDF   *g_cfg = NULL;
char  *g_config_filename = NULL;
time_t g_ctime = 0;

static bool dad_call_me_back = false;

static void* _bench_do(void *arg)
{
    MERR *err;

    int threadsn = *(int*)arg;

    char fname[PATH_MAX] = {0};
    if (mdf_get_bool_value(g_cfg, "trace.tostdout", false)) sprintf(fname, "-");
    else snprintf(fname, sizeof(fname), "logs/%03d.log", threadsn);
    mtc_mt_init(fname, mtc_level_str2int(mdf_get_value(g_cfg, "trace.level", "debug")));
    mtc_mt_dbg("I am bencher %d", threadsn);

    int robo = mdf_get_int_value(g_cfg, "retry_onerror_before_over", 0);

    WB_ROOM *wb_room;
    MDF *cnode, *sitenode;

    wb_room = NULL;
    cnode = mdf_get_child(g_cfg, "sites");

    /*
     * 1. every site
     */
    while (cnode) {
        char *sitename = mdf_get_value(cnode, "name", "unknown");

        mtc_mt_dbg("init site %s", sitename);

        /* memory leak, but don't care */
        mdf_init(&sitenode);
        err = mdf_json_import_filef(sitenode, "%s.json", sitename);
        RETURN_V_NOK(err, NULL);

        int usernumber = mdf_get_int_value(sitenode, "room.usernumber", 4);
        int usersn = 0;

        /*
         * 2. every user
         */
        MDF *unode = mdf_get_nodef(sitenode, "users[%d][0]", threadsn);
        while (unode) {
            char *uid = mdf_get_value(unode, "uuid", NULL);
            char *ticket = mdf_get_value(unode, "ticket", NULL);
            usersn = usersn % usernumber;

            if (usersn == 0) {
                wb_room = user_room_open(wb_room, uid, ticket, sitenode, usersn);
            } else {
                user_room_add(wb_room, uid, ticket, sitenode, usersn, usernumber);
            }

            usersn++;
            unode = mdf_node_next(unode);
        }

        cnode = mdf_node_next(cnode);
    }

    /*
     * 3. poll websocket fds
     */
    unsigned char recv_buf[BUFFER_LEN];
    int efd = 0;
    struct epoll_event *events = poll_create(&efd);
    if (!events || efd <= 0) {
        mtc_mt_err("create poll fd failure");
        return NULL;
    }

    while (!dad_call_me_back) {
        user_room_check(wb_room, efd, robo);

        if (user_room_over(wb_room, robo)) {
            user_room_status_out(wb_room);
            dad_call_me_back = true;
        }

        poll_do(efd, events, recv_buf, BUFFER_LEN);
    }

    poll_destroy(efd, events);
    user_room_destroy(wb_room);

    return NULL;
}

int main(int argc, char *argv[])
{
    MERR *err;

    mdf_init(&g_cfg);
    g_config_filename = argv[1] ? argv[1] : DEFAULT_CONFIGFILE;

    err = mdf_json_import_file(g_cfg, g_config_filename);
    DIE_NOK(err);

    int threadnum = mdf_get_int_value(g_cfg, "max_threadnum", 1);
    if (threadnum > MAX_THREAD_NUM) threadnum = MAX_THREAD_NUM;

    mtc_init("-", mtc_level_str2int(mdf_get_value(g_cfg, "trace.level", "debug")));

    set_conio_terminal_mode();

    pthread_t *threads[threadnum];
    int num[threadnum];
    for (int i = 0; i < threadnum; i++) {
        threads[i] = mos_calloc(1, sizeof(pthread_t));
        num[i] = i;
    }

    dad_call_me_back = false;

    g_ctime = time(NULL);

    for (int i = 0; i < threadnum; i++) {
        pthread_create(threads[i], NULL, _bench_do, (void*)&num[i]);
    }

    printf("wsbench running. 'q' to exit.\n");

    while (!dad_call_me_back) {
        if (kbdhit()) {
            char c = getchar();

            if (c == 'q') {
                dad_call_me_back = true;
            }
        }

        g_ctime = time(NULL);
    }

    for (int i = 0; i < threadnum; i++) {
        pthread_join(*threads[i], NULL);
        mos_free(threads[i]);
    }

    return 0;
}
