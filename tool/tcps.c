#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>        /* htonls() and friends */
#include <netinet/in.h>        /* INET stuff */
#include <netinet/udp.h>    /* UDP stuff */
#include <sys/ioctl.h>
#include <fcntl.h>

#define MAXPENDING 5    /* Maximum outstanding connection requests */
#define SBSIZE (68 * 1024)
static unsigned char static_buf[SBSIZE];


void HandleTCPClient(int clntSocket)
{
    char echoBuffer[SBSIZE];        /* Buffer for echo string */
    int recvMsgSize;                    /* Size of received message */

    int old_flags;

    old_flags = fcntl(clntSocket, F_GETFL, 0);

    if (!(old_flags & O_NONBLOCK)) {
        old_flags |= O_NONBLOCK;
    }
    //fcntl(clntSocket, F_SETFL, old_flags);

    sleep(1);
    /* Receive message from client */
    if ((recvMsgSize = recv(clntSocket, echoBuffer, SBSIZE, 0)) < 0) {
        perror("recv() failed");
        return;
    }

    /* Send received string and receive again until end of transmission */
    while (recvMsgSize > 0)      /* zero indicates end of transmission */
    {
        printf("recevied %s\n", echoBuffer);
        //sleep(10);

        /* Echo message back to client */
        if (send(clntSocket, echoBuffer, recvMsgSize, 0) != recvMsgSize) {
            printf("send() failed");
            return;
        }

        //sleep(1);
        /* See if there is more data to receive */
        if ((recvMsgSize = recv(clntSocket, echoBuffer, SBSIZE, 0)) < 0) {
            printf("recv() failed");
            return;
        }
    }

    printf("%d\n", recvMsgSize);

    close(clntSocket);    /* Close client socket */
}

int main()
{
    int fd, fdclient, rv;
    struct sockaddr_in srvsa;
    struct in_addr ia;

    //rv = inet_pton(AF_INET, "127.0.0.1", &ia);
    rv = inet_pton(AF_INET, "0.0.0.0", &ia);
    if (rv <= 0)
        return -1;

    srvsa.sin_family = AF_INET;
    srvsa.sin_addr.s_addr = ia.s_addr;
    srvsa.sin_port = htons(3950);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;

    rv = bind(fd, (struct sockaddr *) &srvsa, sizeof(srvsa));
    if (rv < 0) {
        close(fd);
        perror("bind");
        return -1;
    }

    /* Mark the socket so it will listen for incoming connections */
    if (listen(fd, MAXPENDING) < 0) {
        perror("listen");
        return -1;
    }

    for (;;) /* Run forever */
    {
        struct sockaddr_in clisa;
        socklen_t clilen;
        clilen = sizeof(clisa);

        /* Wait for a client to connect */
        if ((fdclient = accept(fd, (struct sockaddr *) &clisa, &clilen)) < 0) {
            perror("accept");
            return -1;
        }

        /* clntSock is connected to a client! */

        printf("Handling client %s\n", inet_ntoa(clisa.sin_addr));

        HandleTCPClient(fdclient);
    }

    close(fd);
    return 0;
}
