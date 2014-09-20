/*
 * server.c
 * Server implementation for Programming Assignment 1: 4119
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


fd_set master;    // master file descriptor list
fd_set read_fds;  // temp file descriptor list for select()

void cleanExit() {
    exit(0);
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}
/*/ get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}*/

int main(int argc, char *argv[])
{
    int portnum;      // portnum
   
    int fdmax;        // maximum file descriptor number
    
    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_in serv_addr, cli_addr; // client address
    socklen_t addrlen;
    
    char buf[256];      // buffer for client data
    char relayMsg[300]; //buffer for message not sure if we need this
    int i,j;
    int nbytes;         //bytes sent
    
    //Get port number from command line
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    portnum = atoi(argv[1]);
    
    FD_ZERO(&master);    //clear file descriptors
    FD_ZERO(&read_fds);
    
    //set up server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portnum);
    
    //get socket
    listener = socket(PF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        error("ERROR opening socket");
    //reuse port num yes
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    
    // bind to socket
    if (bind(listener, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }
    
    
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }
    
    // add the listener to the master set
    FD_SET(listener, &master);
    
    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one
    
    //server loop
    while(1) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { //
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof cli_addr;
                    newfd = accept(listener,
                                   (struct sockaddr *)&cli_addr,
                                   &addrlen);
                    
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from s on "
                               "socket %d\n",newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        //reset relay message to null
                        memset(relayMsg, 0, 300);
                        sprintf(relayMsg, "%d: ", i);
                        strncat(relayMsg, buf, nbytes);
                        printf("%s", relayMsg);
                        
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (send(j, relayMsg, strlen(relayMsg), 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END while
    
    return 0;
}