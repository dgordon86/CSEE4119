4119: Socket Programming
========

The code for the socket programming project is broken into two files: client.c and server.c.
The idea was to make the client "dumb" and simply relay messages between the end user and the server. All of the logic
(such as authentication, timeouts, blocking, and commands) are handled on the server side.

Development Environment
--------
The programming assignment was implemented using C on Mac OS: x86_64-apple-darwin13.4.0. Xcode text editor was used and command line to compile and run.

How to Run
--------------
cd in to `prog1` and run

    make

Once the working copy is created, you can use the normal ``vlt up`` and ``vlt ci`` commands.

Specifying CRX Host/Port
------------------------

The CRX host and port can be specified on the command line with:
mvn -Dcrx.host=otherhost -Dcrx.port=5502 <goals>


