#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[])
{
    int sockfd = 0, n_bytes = 0;
    char recvBuff[BUF_SIZE];
    struct sockaddr_in addr, s_addr;

    if(argc != 2)
    {
        printf("\n Usage: %s <ip of server> \n",argv[0]);
        return 1;
    }

    printf("Client pid: %u. Press enter to start.\n", getpid());
    getchar();

    // connect to socket

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    memset(&addr, '0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);

    if(inet_pton(AF_INET, argv[1], &addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    }

    s_addr = addr;

    if( connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return 1;
    }

    // read from STDIN

    memset(recvBuff, '0', BUF_SIZE);
    write(1, "Message: ", 9);
    n_bytes = read(0, recvBuff, BUF_SIZE);

    // write to Socket
    write(sockfd, recvBuff, n_bytes);
    // sendto(sockfd, recvBuff, n_bytes, 0, (struct sockaddr *) &addr, sizeof(addr));

    // read from Socket
    memset(recvBuff, '0', BUF_SIZE);
    n_bytes = read(sockfd, recvBuff, BUF_SIZE);
    write(1, recvBuff, n_bytes);

    close(sockfd);

    return 0;
}
