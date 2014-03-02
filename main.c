/* 
 * File:   main.c
 * Author: max
 *
 * Created on 1 Март 2014 г., 19:24
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#define WAITING_CONNECTIONS 10
#define MESSAGE_SIZE 1
#define PORT "5680"

/*
 * Entry point
 */
int main(int argc, char** argv) {

    fd_set connections, read_fds;
    int status, listener, newfd, fdmax;
    struct addrinfo hints, *result;
    struct sockaddr_storage remoteaddr;
    char stradr [INET_ADDRSTRLEN];
    char buf [MESSAGE_SIZE];

    int i, j, rv, nbytes;

    FD_ZERO(&connections);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // неважно, IPv4 или IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream-sockets
    hints.ai_flags = AI_PASSIVE;

    // obtain address info
    if ((status = getaddrinfo(NULL, PORT, &hints, &result)) != 0) {
        printf("getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // socket creation
    if ((listener = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == -1) {
        printf("socket error: %s\n", strerror(errno));
        exit(2);
    }

    freeaddrinfo(result);

    // can reuse
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int));

    // assign the address to the socket
    if ((bind(listener, result->ai_addr, result->ai_addrlen)) == -1) {
        printf("bind error: %s\n", strerror(errno));
        exit(3);
    }

    // listen
    if ((listen(listener, WAITING_CONNECTIONS)) == -1) {
        printf("listen error: %s\n", strerror(errno));
        exit(4);
    }

    // add listener to connections list
    FD_SET(listener, &connections);
    fdmax = listener;

    socklen_t s;

    while (1) {

        read_fds = connections;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        // all connections
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    // обрабатываем новые соединения
                    s = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *) &remoteaddr, &s);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &connections); // добавляем в мастер-сет
                        if (newfd > fdmax) { // продолжаем отслеживать самый большой номер дескиптора
                            fdmax = newfd;
                        }
                        inet_ntop(remoteaddr.ss_family, &(((struct sockaddr_in *) &remoteaddr)->sin_addr), stradr, sizeof stradr);
                        printf("new connection from %s on socket %d\n", stradr, newfd);
                    }
                } else {
                    // обрабатываем данные клиента
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // получена ошибка или соединение закрыто клиентом
                        if (nbytes == 0) {
                            // соединение закрыто
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &connections); // удаляем из мастер-сета
                    } else {
                        // у нас есть какие-то данные от клиента
                        printf("new broadcast with message: %s!\n", buf);
                        for (j = 0; j <= fdmax; j++) {
                            // отсылаем данные всем!
                            if (FD_ISSET(j, &connections)) {
                                // кроме слушающего сокета и клиента, от которого данные пришли
                                if (j != listener && j != i) {
                                    if (send(j, buf, nbytes, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } 
    }
}