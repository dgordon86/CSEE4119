4119: Socket Programming
========

The code for the socket programming project is broken into two files: client.c and server.c.
The idea was to make the client "dumb" and simply relay messages between the end user and the server. All of the logic
(such as authentication, timeouts, blocking, and commands) are handled on the server side. 

Important variables that are expected to be modified 

Development Environment
--------
The programming assignment was implemented using C on Mac OS: x86_64-apple-darwin13.4.0. Xcode text editor was used and command line to compile and run.
Code was source controlled using git at https://github.com/dgordon86/CSEE4119/tree/master/prog1.

How to Run
--------------
cd in to `prog1` and run the command `make`

    $ make
    gcc -o Server server.c
    gcc -o Client client.c
    
This will compile both the server and client.

Next start the server with the command:
    ./Server <port-num>
In another terminal connect to the server by starting the client with the command:
    ./Client <ip-address> <port-num>
The client will immediately be prompted to authenticate
    > Please authenticate
    > User: <type username>
    > Password: <type password>
    > Welcome to simple chat server!

Sample commands
------------------------

Once the client has authenticated and recieve the welcome message: `Welcome to simple chat server!`, the client
is able to run any of the commands defined in the assignment. For clarity any message recieved from the server is
prefixed with a `>` character. Here is some basic interaction:

| Terminal | Description |
| -------- |:------------|
|> Welcome to simple chat server! | //logged in as foobar |
| whoelse | //run command whoelse |

    > columbia, facebook                        //server says columbia and facebook currently logged on
    wholasthr                                   //run command wholasthr
    > columbia, seas, facebook                  //server says columbia, seas, and facebook have logged in in past hour
    message columbia hello                      //send private message to columbia 
    > columbia: hey hows it going               //columbia responds with message 'hey hows it going'
    message seas hello                          //send private message to seas
    > User could not be found. Message not sent.    //server responds with error since user is not currently online
    broadcast getting lunch ttyl!               //run broadcast command (message sent to columbia and facebook)
    logout                                      //run logout command
    > Goodbye!                                  //server responds with goodbye message
    > Disconnected from server: exiting         //server hangs up connection and Client program terminates
