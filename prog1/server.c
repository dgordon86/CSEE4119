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

struct User
{
    char  name[50];
    char  password[50];
    int   socket;
    int   loggedin; //boolean if logged in, intially set to 0
    int   lfailures;
};

struct User allusers[15];
int totalUsers = 15; //total number of users accepted

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

int getusers(char *filename, FILE *fp);

void msg_receiv(int sockfd, char *msg);
void authenticate(int sockfd, char *authinfo);
struct User * findUserByName(char *username);
struct User * findUserbySocket(int sockfd);
void sendMessage(int sockfd, char *msg);
void whoelse(int sockfd);
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
    
    FILE *userfile;
    
    //Get port number from command line
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    portnum = atoi(argv[1]);
    
    //get users
    totalUsers = getusers("user_pass.txt", userfile);
    /*for (int u =0; u < totalUsers; u++)
    {
        printf("%d: %s-%s\n", u, allusers[u].name, allusers[u].password);
    }*/
    
    
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
                        
                        //prompt user for username/password
                        if (FD_ISSET(newfd, &master)) {
                            if (send(newfd, "Authenticate:\n", 14, 0) == -1) {
                                perror("send");
                            }
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
                        printf("%s\n", buf);
                        memset(relayMsg, 0, 300);
                        strncpy(relayMsg, buf, nbytes);
                        msg_receiv(i, relayMsg);
                        memset(buf, 0, 256);
                        /*
                        if (FD_ISSET(i, &master)) {
                            if (send(i, "Welcome to simple chat server!\n", strlen("Welcome to simple chat server!\n"), 0) == -1) {
                                perror("send");
                            }
                        }*/
                        /*struct User *newuser;
                        newuser = findUserbySocket(i);
                        
                        printf("Password of new user: %s\n", newuser->password);*/
                        // we got some data from a client
                        /*for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (send(j, relayMsg, strlen(relayMsg), 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }*/
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END while
    
    return 0;
}

void msg_receiv(int sockfd, char *msg) {
    
    char command[100];
    int i = 0;
    while((msg[i] != ' ') && (msg[i] != '\0')) {
        command[i] = msg[i];
        i++;
    }
    //authentication
    if(strcmp(command, "auth") == 0) {
        authenticate(sockfd, msg);
    }else if(strcmp(command, "whoelse") == 0) {
        whoelse(sockfd);
    } else {
       sendMessage(sockfd, "Unrecoginized command.\n");
    }
    
}

void whoelse(int sockfd) {
    printf("yeah looking for others");
}

/*got auth command, authinfo should contain a username and password string */
void authenticate(int sockfd, char *authinfo) {
    
    struct User *user;
    char tmp[256];
    //char password[100];
    memset(tmp, 0, 256);
    
    int i = 5; //start at 5 because command was 'auth '
    while(authinfo[i] != ' ') {
        tmp[i -5] = authinfo[i];
        i++;
    }
    
    user = findUserByName(tmp);
    if(user != NULL) {
        memset(tmp, 0, 256);
        //get password
        int j = ++i;
        while (authinfo[i] != '\0') {
            tmp[i -j] = authinfo[i];
            i++;
        }
        
        //compare passwords
        if(strcmp(tmp, user->password) == 0) {
            //need to put in stuff for if user is already logged in
            
            //set login for user
            user->socket = sockfd;
            user->loggedin = 1; //set login to true
            user->lfailures = 0;
            sendMessage(sockfd, "Welcome to simple chat server!");
        } else {
            if(user->lfailures == 2) {
                sendMessage(sockfd, "Too many incorrect logins.");
                close(sockfd); // bye!
                FD_CLR(sockfd, &master); // remove from master set
                user->lfailures = 0;
                //set block??
            } else {
                user->lfailures++;
                sendMessage(sockfd, "Incorrect username/password combination.\n");
                //printf("incorrect password %d\n", user->lfailures);
            }
            
        }
       
    }
    //return "sup biatch";
    
}

void sendMessage(int sockfd, char *msg) {
    if (FD_ISSET(sockfd, &master)) {
        if (send(sockfd, msg, strlen(msg), 0) == -1) {
            perror("send");
        }
    }
}

struct User * findUserByName(char *username) {
    int i;
    for (i=0; i < totalUsers; i++) {
        if(strcmp(allusers[i].name, username) == 0) {
            //printf("found user!\n");
            return &allusers[i];
        }
    }
    return NULL;
}

struct User * findUserbySocket(int sockfd) {
    int i;
    for (i=0; i < totalUsers; i++) {
        if(allusers[i].socket == sockfd) {
            //printf("found user!\n");
            return &allusers[i];
        }
    }
    return NULL;
}
int getusers(char *filename, FILE *fp) {
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int spc, i, j;
    //get list of users
    fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    
    i = spc = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        //printf("Retrieved line of length %zu :\n", read);
        //printf("%s\n", line);
        if(read < 100 && i <= totalUsers) {
            spc = strcspn (line," ");
            
            //set user name
            strncpy(allusers[i].name, line, spc);
            allusers[i].name[spc] = '\0';
            
            //set password
            for(j = (spc +1); j < read; j++) {
                allusers[i].password[j-spc-1] = line[j];
            }
            allusers[i].password[j-spc-2] = '\0';
            
            allusers[i].socket = allusers[i].loggedin =
            allusers[i].lfailures = 0;
            
            i++;
        } else {
            error("An error occured while parsing users from text file\n");
        }
    }
    
    fclose(fp);
    if (line)
        free(line);
    
    return i;
}














