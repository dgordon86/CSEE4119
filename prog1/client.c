#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>

fd_set read_fds, write_fds; //file descriptor list contains stdin and socket connection to server

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    struct timeval tv;
    int sockfd, portnum, nbytes, ipnum, i;
    int fdmax;          //max file descriptor
    char *ipstring;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s ipaddress portnum\n", argv[0]);
       exit(0);
    }
    ipstring = argv[1];
    portnum = atoi(argv[2]);
   
    memset(&serv_addr, 0, sizeof(serv_addr));
    ipnum = inet_addr(ipstring);
    if(ipnum < 0) {
        error("ERROR ip address appears invalid");
    }
    //set up server address
    serv_addr.sin_addr.s_addr = ipnum;
    inet_pton(AF_INET, argv[1], &(serv_addr.sin_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portnum);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
   /* FD_ZERO(&read_fds);    //clear file descriptors
    FD_SET(sockfd, &read_fds);    //add server socket
    FD_SET(STDIN_FILENO, &read_fds);
    fdmax = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
    */
    int serverUp = 1;
    while(serverUp) {
        FD_ZERO(&read_fds); //clear file descriptors
        FD_ZERO(&write_fds);
        FD_SET(sockfd, &read_fds);    //add server socket
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &write_fds);
        
        if (select(sockfd+1, &read_fds, &write_fds, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
       
        printf("Command:\n");
        //check if data from Server
        if(FD_ISSET(sockfd, &read_fds)) {
            // handle data server
            if ((nbytes = recv(sockfd, buffer, sizeof buffer, 0)) <= 0) {
                // got error or connection closed by client
                if (nbytes == 0) {
                    // connection closed
                    printf("Server hung up: exiting\n");
                } else {
                    error("recv");
                }
                //close connection and prepare to exit
                serverUp = 0;
                close(sockfd);
                FD_CLR(sockfd, &read_fds);
            }
            else{
                printf("%s", buffer);
                memset(&buffer, 0, sizeof(buffer));//reset buffer to zero
            }
        }
        else if(FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer,sizeof(buffer), stdin)) {
                //printf("Length: %lu\n" ,strlen(buffer));
                if(FD_ISSET(sockfd, &write_fds)) {
                    if (send(sockfd, buffer, strlen(buffer), 0) == -1) {
                        error("send");
                    }
                }
                memset(&buffer, 0, sizeof(buffer));
            }
        }
       /* printf("Please enter the message: ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        n = write(sockfd,buffer,strlen(buffer));
        if (n < 0)
            error("ERROR writing to socket");
    
        bzero(buffer,256);
        n = read(sockfd,buffer,255);
        if (n < 0)
            error("ERROR reading from socket");
        printf("%s\n",buffer);*/
        
    }
    close(sockfd);
   
    return 0;
}
