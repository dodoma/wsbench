#ifndef __UTIL_H__
#define __UTIL_H__

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
