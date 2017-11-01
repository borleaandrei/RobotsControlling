#ifndef SOCKETS_H
#define SOCKETS_H

#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include <unistd.h>

int connect_connection(char *ip, unsigned int port);
int send_connection(char command);
int close_connection(void);

struct sockaddr_in server;
int sock;

int connect_connection(char *ip, unsigned int port)
{
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket\n");
    }
    printf("Socket created\n");

    server.sin_addr.s_addr = inet_addr( ip );
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        printf("connect failed. Error\n");
        return 1;
    }

    printf("Connected\n");

    return 0;
}

int send_connection(char command)
{
  //Send some data
  if( send(sock , &command, 1, 0) < 0 )
  {
      printf("Send failed\n");
      return 1;
  }
  return 0;
}

int close_connection(void)
{
  close(sock);
  return 0;
}

#endif
