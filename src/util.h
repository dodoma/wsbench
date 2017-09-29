#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef DEBUG_MSG
#define MSG_DUMP(pre, p, psize)                                     \
    do {                                                            \
        if ((ssize_t)(psize) > 0) {                                 \
            char zstra[psize*2+1];                                  \
            mstr_bin2str((uint8_t*)p, psize, zstra);                \
            mtc_mt_dbg("%s%zu %s", pre, (size_t)(psize), zstra);    \
        }                                                           \
    } while (0)
#else
#define MSG_DUMP(pre, p, psize)
#endif

void set_conio_terminal_mode();
int  kbdhit();


/*
  typedef union epoll_data
  {
  void        *ptr;
  int          fd;
  __uint32_t   u32;
  __uint64_t   u64;
  } epoll_data_t;

  struct epoll_event
  {
  __uint32_t   events;
  epoll_data_t data;
  };
*/

#endif
