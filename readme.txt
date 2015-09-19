/****************************************************************************
* readme.txt	                                                          	*
*                                                                           *
* Description: This file servers as the readme file for the simple 		    *
*			   server lisod.												*
*                                                                           *
* Author: Yunfan Ye <yunfany@andrew.cmu.edu>                         		*
*                                                                           *
*****************************************************************************/


/***********************************************************http server**********************************************************************/
This server builds on the simple echo server. The simple echo server exposes a http handler API, passing the request to the handler and retrieving response from the handler. Thus, the underlying infrastructure is the same as the simple echo server. For the HTTP handler, we use lexer and yacc as HTTP parser. Currently, it mainly supports HEAD and GET request, with only header information to post request. Besides, it will return proper error message when encountering errors. We mainly focus on HTTP/1.1 request, with 505 NOT SUPPORTED error responding to HTTP/1.0 client.


/**********************************************************simle echo server*****************************************************************/
This server is built on the provided starter code "echo_server.c". The server runs on ports specified by command line arguments and simply write back anything sent to it by connected clients. It supports concurrent clients by an event-drivin model, i.e. "select" function in c. Specifically, it first sets up a listen socket, and add it to the readset. If any time there is incoming connection, accept the connection and add it to the readset. If the server should respond to the client, it store the data in the corresponding write buf, and add it to the writeset. More specifically, the server will maintain two single-way lists. In every loop, set the bit corresponding to the socket in the list for the readset and writeset, respectively. 
