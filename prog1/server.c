/*
 * server.c
 * Server implementation for Programming Assignment 1: 4119
 * @author Daniel Gordon - dmg2173@columbia.edu
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
#include <time.h>

#define BLOCK_TIME 60 //block time is defined in seconds
#define LAST_HOUR 3600 //LAST_HOUR is defined in seconds, LAST_HOUR must be an integer greater than 0
#define TIME_OUT 1800 //TIME_OUT is defined in seconds, TIME_OUT must be an integer greater than 0

fd_set master;    // master file descriptor list
fd_set read_fds;  // temp file descriptor list for select()
int fdmax;
int listener;   //listener socket

//an object to hold user, including name and password
struct User
{
    char  name[50];
    char  password[50];
    int   socket;
    int   loggedin; //boolean if logged in, intially set to 0
    time_t logout_time;
};

//object holds additional data from socket for easy access
struct Client
{
    int sockfd;
    int ipaddress;
    char name[50];
    int attempts;
    time_t connect_time;
};

//helps maintain a list of blocked users after 3 consecutive login attempts
struct BlockedUser {
    char name[50];
    int ipaddress;
    time_t sec;
};

struct BlockedUser blocked[20];
int totalBlocks=0;

struct Client currconns[20];
int totalConns= 0;

struct User allusers[15];
int totalUsers = 15; //total number of users accepted

char relayMsg[300]; //relay message buffer

void cleanExit() {
    exit(0);
}

/* Display error messages to terminal*/
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

//populates list of users from 'user_pass.txt' file
int getusers(char *filename, FILE *fp);

/*after a connection has been handles all subsequent 
 communication from clients based on command */
void msg_receiv(int sockfd, char *msg);

/* handles authentication logic for a user login.
 includes tracking failed login attempts and adding
 user/ipaddress to blocked list */
void authenticate(int sockfd, char *authinfo);

//utility method to find a corresponding client from socket
struct Client * findClientBySocket(int sockfd);

//removes client from tracked list
void removeClient(int sockfd);

//utility method for finding a user based on name
struct User * findUserByName(char *username);

//utility method for finding a user based on socket
struct User * findUserbySocket(int sockfd);

//conveinence method to send a message to a socket
void sendMessage(int sockfd, char *msg);

/* convenience method to disconnect a socket,
 and clean up all references */
void logout(int sockfd, char *optmsg);

/* Implementation of whoelse command.
 Displays name of other connected users*/
void whoelse(int sockfd);

/*Implementation of wholasthr command.
 Displays name of only those users that connected
 within the last hour */
void wholasthr(int sockfd);

/* Implementation of broadcast command.
 Broadcasts a message to all users */
void broadcast(int sockfd, char *bmsg);

/* Implementation of message command.
 Private message is sent to user */
void message(int sockfd, char *bmsg);

//convenience method to block a user for a given ipaddress
void blockuser(char *username, int ipaddress);

//check if a user at a given ipaddress is blocked
int isblocked(char *username, int ipaddress);

/*check for all clients who have been inactive for longer
 than TIME_OUT period */
void checkInactive();

int main(int argc, char *argv[])
{
    int portnum;      // portnum
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_in serv_addr, cli_addr; // server and client address
    socklen_t addrlen;
    
    char buf[256];      // buffer for client data
    int i,j;
    int nbytes;         //bytes sent
    
    
    
    //Get port number from command line
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    portnum = atoi(argv[1]);
    
    //get users
    FILE *userfile;
    totalUsers = getusers("user_pass.txt", userfile);
    
    FD_ZERO(&master);    //clear file descriptors
    FD_ZERO(&read_fds);
    
    //set up server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portnum);
    
    //get socket based on command line arg
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
    
    //open up socket based on command line arg
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }
    
    // add the listener to the master set
    FD_SET(listener, &master);
    
    // keep track of the largest file descriptor
    fdmax = listener;
    
    //timeout select block to check for inactivity
    struct timeval tv;
    tv.tv_sec = TIME_OUT;
    
    //server loop
    while(1) {
         memset(relayMsg, 0, 300);
         read_fds = master; // copy new status of connections
        
        if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
            perror("select");
            exit(4);
        }
        
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { //
                if (i == listener) {
                    // handle new client
                    addrlen = sizeof cli_addr;
                    newfd = accept(listener,
                                   (struct sockaddr *)&cli_addr,
                                   &addrlen);
                    
                    //printf("IP: %d\n", cli_addr.sin_addr.s_addr);
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        //track ipaddress of connection
                        if ( totalConns < 20) {
                            currconns[totalConns].sockfd = newfd;
                            currconns[totalConns].ipaddress = cli_addr.sin_addr.s_addr;
                            currconns[totalConns].connect_time = time (NULL);
                            totalConns++;
                        } else {
                            logout(newfd, "Too Many connections");
                        }
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        
                        //prompt user for username/password
                        if (FD_ISSET(newfd, &master)) {
                            if (send(newfd, "Please authenticate", 20, 0) == -1) {
                                perror("send");
                            }
                        }
                        /*printf("new connection from s on "
                               "socket %d\n",newfd);*/
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            //printf("socket %d hung up\n", i);
                            
                        } else {
                            perror("recv");
                        }
                        
                        logout(i, "");
                    } else {
                        //update client connection time
                        struct Client *client;
                        client = findClientBySocket(i);
                        if(client != NULL) {
                            client->connect_time = time(NULL);
                        }
                       // printf("%s\n", buf);
                        msg_receiv(i, buf);
                        memset(buf, 0, 256);
            
                    }
                }
            }
        }
        
        checkInactive();
        
    }
    return 0;
}

/* check for inactive clients based TIME_OUT */
void checkInactive() {
    time_t currtime = time(NULL);
    int i;
    for(i=0; i < totalConns; i++) {
        
        if((currtime - currconns[i].connect_time) > (TIME_OUT)) {
            //printf("%d: time out occurred: %lu\n",currconns[i].sockfd, currconns[i].connect_time);
            logout(currconns[i].sockfd, "Time out due to inactivity.\n");
        }
        
    }
   
}

/* msg_receiv
 * @param sockfd - the socket the message originated from
 * @param msg - the message the client set
 * message is parsed looking for commands recognized by server
 */
void msg_receiv(int sockfd, char *msg) {
    
    char command[100];
    memset(command, 0, 100);
    int i = 0;
    while((msg[i] != ' ') && (msg[i] != '\0')) {
        command[i] = msg[i];
        i++;
    }
    //if else chain to determine what command was entered by client
    if(strcmp(command, "auth") == 0) {
        //printf("trying to authenticate\n");
        authenticate(sockfd, msg);
    }else if(strcmp(command, "whoelse") == 0) {
        whoelse(sockfd);
    }else if(strcmp(command, "wholasthr") == 0) {
        wholasthr(sockfd);
    } else if(strcmp(command, "broadcast") == 0) {
        broadcast(sockfd, msg);
    }else if(strcmp(command, "message") == 0) {
        message(sockfd, msg);
    }else if(strcmp(command, "logout") == 0) {
        logout(sockfd, "Goodbye!\n");
    }else {
       sendMessage(sockfd, "Unrecoginized command.\n");
    }
    
}

/* message
 * @param sockfd - socket message originated from
 * @param bmsg - the message to be sent should include 
                a username of recipient
 * Implementation of message command.
 * Private message is sent to user. b */
void message(int sockfd, char *bmsg) {
    
    char tousername[100];
    memset(tousername, 0, 100);
    struct User *fromuser = findUserbySocket(sockfd);
    struct User *touser;
    
    int i = 8;
    if (fromuser != NULL) {
        while (bmsg[i] != ' ' && bmsg[i] != '\0') {
            tousername[i - 8] = bmsg[i];
            i++;
        }
    }
    touser = findUserByName(tousername);
    if(touser != NULL && touser->loggedin) {
        strcpy(relayMsg, fromuser->name);
        strcat(relayMsg, ": ");
        int k = strlen(relayMsg);
        int j = ++i;
        if (i < strlen(bmsg)) {
            char *msgPointer = bmsg + i;
            strncat(relayMsg, msgPointer, strlen(bmsg) - i);
            strcat(relayMsg, "\n");
            sendMessage(touser->socket, relayMsg);
        } else {
            sendMessage(sockfd, "No message body, nothing sent.\n");
        }
    } else {
        sendMessage(sockfd, "User could not be found. Message not sent.\n");
    }
    
}

/* broadcast
 * @param sockfd - socket message originated from
 * @param bmsg - the message to be broadcast
 * Implementation of broadcast command.
 * Broadcasts a message to all users */
void broadcast(int sockfd, char *bmsg) {
    struct User *buser = findUserbySocket(sockfd);
    struct User *ouser;
    int i=10; //start after broadcast command
    int j = 0;
    if(buser != NULL) {
        strcpy(relayMsg, buser->name);
        strcat(relayMsg, ": ");
        j = strlen(relayMsg);
        char *msgPointer = bmsg + i;
        strncat(relayMsg, msgPointer, strlen(bmsg) - i);
      
        strcat(relayMsg, "\n");
        //broadcast to everyone except us and listener
        for(j = 0; j <= fdmax; j++) {
            // send to everyone!
            if (FD_ISSET(j, &master)) {
                if (j != listener && j != sockfd) {
                    //only broadcast to users who have been logged in
                    ouser = findUserbySocket(j);
                    if(ouser != NULL && ouser->loggedin) {
                        if (send(j, relayMsg, strlen(relayMsg), 0) == -1) {
                            perror("send");
                        }
                    }
                }
            }
        }
    }
}

/* whoelse
 * @param sockfd - socket command originated from
 * Implementation of whoelse command.
 * Displays name of other connected users*/
void whoelse(int sockfd) {
    
    int i;
    int foundsome = 0;
    for (i=0; i < totalUsers; i++) {
        if(allusers[i].loggedin == 1 && allusers[i].socket != sockfd) {
            //printf("%s\n", allusers[i].name);
            strcat(relayMsg, allusers[i].name);
            strcat(relayMsg, ", ");
            foundsome = 1;
        }
    }

    
    if(foundsome) {
        i = strlen(relayMsg);
        relayMsg[i -2] = '\n';
        relayMsg[i -1] = '\0';
        sendMessage(sockfd, relayMsg);
    } else {
        sendMessage(sockfd, "No other users\n");
    }
}

/* wholasthr
 * @param sockfd - socket command originated from
 * Implementation of wholasthr command.
 * Displays name of only those users that connected
 * within the last hour */
void wholasthr(int sockfd) {
    int i;
    int foundsome = 0;
    time_t sec = time(NULL);
    for (i=0; i < totalUsers; i++) {
        //if not ourselves
        if(allusers[i].socket != sockfd){
            if(allusers[i].loggedin == 1 || ((sec - allusers[i].logout_time) < (LAST_HOUR *60)) ) {
            strcat(relayMsg, allusers[i].name);
            strcat(relayMsg, ", ");
            foundsome = 1;
            }
        }
    }
    
    if(foundsome) {
        i = strlen(relayMsg);
        relayMsg[i -2] = '\n';
        relayMsg[i -1] = '\0';
        sendMessage(sockfd, relayMsg);
    } else {
        sendMessage(sockfd, "No other users\n");
    }
    
}

/* logout
 * @param sockfd - socket to disconnect
 * @param optmsg - optional message to send to socket 
                   before disconnecting
 * convenience method to disconnect a socket,
 * and clean up all references */
void logout(int sockfd, char *optmsg) {
    //optionally send message
    if(strlen (optmsg) > 0) {
        sendMessage(sockfd, optmsg);
    }
    
    //reset user
    struct User *rmUser = findUserbySocket(sockfd);
    if(rmUser != NULL) {
        rmUser->loggedin = 0;
        rmUser->socket = 0;
        
        //record logout time
        rmUser->logout_time = time (NULL);
    }
    
    //remove client from list
    removeClient(sockfd);
    
    //close socket
    close(sockfd); // bye!
    FD_CLR(sockfd, &master); // remove from master set
}

/* authenticate
 * @param sockfd - socket command originated from
 * @param authinfo - string should contain username & password
 * handles authentication logic for a user login.
 * includes tracking failed login attempts and adding
 * user/ipaddress to blocked list */
void authenticate(int sockfd, char *authinfo) {
    
    struct User *user;
    struct Client *client;
    char tmp[256];
    memset(tmp, 0, 256);
    
    int i = 5; //start at 5 because command was 'auth '
    while(authinfo[i] != ' ') {
        tmp[i -5] = authinfo[i];
        i++;
    }
    
    user = findUserByName(tmp);
    client = findClientBySocket(sockfd);
    if(user != NULL) { //is a valid username
        
        //if user is already logged in just return
        if(user->loggedin == 1) {
            logout(sockfd, "User already logged in.");
            return;
        } else if (isblocked(user->name, client->ipaddress)) {//also check if user is currently blocked
            logout(sockfd, "Blocked for too many incorrect login attempts.");
            return;
        }
        
        memset(tmp, 0, 256);
        //get password
        int j = ++i;
        while (authinfo[i] != '\0') {
            tmp[i -j] = authinfo[i];
            i++;
        }
      
        
        //compare passwords
        if(strcmp(tmp, user->password) == 0) {
                //set login for user
                user->socket = sockfd;
                user->loggedin = 1; //set login to true
                sendMessage(sockfd, "Welcome to simple chat server!");
        } else {
            //incorrect password
           
            if(strcmp(user->name, client->name) == 0) {
                client->attempts++;
                if(client->attempts == 3) {
                    //block now
                    blockuser(user->name, client->ipaddress);
                    logout(sockfd, "Too many incorrect logins.");
                    
                } else {
                 sendMessage(sockfd, "Incorrect password.");
                }
            } else {
                //set username to client
                memset(client->name, 0, 50);
                strncpy(client->name, user->name, strlen(user->name));
                client->attempts = 1;
                sendMessage(sockfd, "Incorrect password.");
                
            }
        }
    } else {
        //username not recognized
       memset(client->name, 0, 50); //resent client username to nothing
        sendMessage(sockfd, "Unrecognized username. Please try again.");
    }
}

/* blockuser
 * @param username - user to block
 * @param ipaddress - ipaddress to block
 */
void blockuser(char *username, int ipaddress) {
    if(totalBlocks < 20) {
        blocked[totalBlocks].ipaddress = ipaddress;
        strcpy(blocked[totalBlocks].name, username);
        blocked[totalBlocks].sec = time (NULL);
        totalBlocks++;
    }
}

/* isblocked
 * @param username - user to check if blocked
 * @param ipaddress - address to check if blocked
 */
int isblocked(char *username, int ipaddress){
    //printf("Called lblockd for %s:%d\n", username, ipaddress);
    int i;
    for(i=0; i < totalBlocks; i++) {
        time_t sec;
        sec = time (NULL);
        if (blocked[i].ipaddress == ipaddress && strcmp(blocked[i].name, username) == 0) {
            
            if ((sec - blocked[i].sec) < BLOCK_TIME) {
                //yes blocked
                return 1;
            } else {
                //block is over remove block and return 0
                //printf("found block that is over\n");
                while (i < totalBlocks) {
                    blocked[i] = blocked[i +1];
                    i++;
                }
                totalBlocks--;
                return 0;
            }
        }
    }
    //no blocks found
    return 0;
    
}

/* removeClient
 * @param sockfd - socket to remove and clean up
 * when disconnecting a socket use this method to clean up
 * tracked list of clients
 */
void removeClient(int sockfd) {
    int i;
    int j;
    int foundrem =0;
    for( i=0; i < totalConns; i++) {
        if(currconns[i].sockfd == sockfd) {
            //j = i +1;
            while( i < totalConns) {
                currconns[i] = currconns[i +1];
                i++;
            }
            totalConns--; //decrement
            break;
        }
    }
}

/* findClientBySocket
 * @param sockfd - returns a client or NULL matching socket
 */
struct Client * findClientBySocket(int sockfd) {
    int i;
    for( i=0; i < totalConns; i++) {
        if(currconns[i].sockfd == sockfd) {
            return &currconns[i];
        }
    }
    return NULL;
}

/* sendMessage
 * @param sockfd - socket to send message to
 * @param msg - to send
 */
void sendMessage(int sockfd, char *msg) {
    if (FD_ISSET(sockfd, &master)) {
        if (send(sockfd, msg, strlen(msg), 0) == -1) {
            perror("send");
        }
    }
}

/* findUserByName
 * @param username - name of user to return
 */
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

/* findUserbySocket
 * @param sockfd - socket to reference userby
 * returns Null if no user found */
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

/* getusers
 * @param filename - string of the filename
 * @param fp - file descriptor to read file into
 * populates allusers array with username and passwords found
 * in file
 */
int getusers(char *filename, FILE *fp) {
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int spc, i, j;
    //get list of users
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("could not file: %s. Exiting...\n", filename);
        exit(EXIT_FAILURE);
    }
    
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
            
            allusers[i].socket = allusers[i].loggedin = 0;
            allusers[i].logout_time = 0;
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














