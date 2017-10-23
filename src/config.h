#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MAX_THREAD_NUM 100
#define MAXEVENTS  1024
#define BUFFER_LEN 10240           /* receive message buffer for each client fd */
#define TCP_PACKET_MAX_LEN 1048576 /* max tcp packet length */

#define DEFAULT_CONFIGFILE "./config.json"

#endif
