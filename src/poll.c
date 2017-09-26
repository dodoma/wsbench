#if 0
        int efd = epoll_create1(0);
        if (efd < 0) {
            mtc_mt_err("create epoll fd failure");
            return NULL;
        }

        struct epoll_event event;
        event.data.fd = wsfd;
        event.events = EPOLLIN | EPOLLET;
        int rv = epoll_ctl(efd, EPOLL_CTL_ADD, wsfd, &event);
        if (rv < 0) {
            mtc_mt_err("add epoll fd failure");
            return NULL;
        }

        struct epoll_event *events = mos_calloc(MAXEVENTS, sizeof(struct epoll_event));
        while (1) {
            int n;

            n = epoll_wait(efd, events, MAXEVENTS, -1);
            for (int i = 0; i < n; i++) {
            }
        }

        mos_free(events);
        close(efd);
#endif
