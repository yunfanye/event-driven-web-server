/****************************************************************************
* echo_server.c                                                           	*
*                                                                           *
* Description: This file built on the provided starter code. contains the C	* 
*			   source code for an echo server. The server runs on a         *
*			   hard-coded port and simply write back anything sent to it by *
*			   connected clients. It supports concurrent clients by an      *
*			   event-drivin model, i.e. "select" function in c.             *
*                                                                           *
* Author: Yunfan Ye <yunfany@andrew.cmu.edu>,                         		*
*                                                                           *
*****************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

/* uncomment the following line to debug */
/* #define DEBUG */

/* Macros */
/* the number of available file descriptors is typically 1024 */
#define ECHO_PORT 9999
#define BUF_SIZE 4096

#define MAX(x, y)  ((x) > (y) ? (x) : (y))
/* remove a wrap node from the linked list
 * prevFdWrap == NULL only if loopFdWrap == head */	
#define REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, head)			\
	if(prevFdWrap) {													\
		prevFdWrap -> next = loopFdWrap -> next;						\
		free(loopFdWrap);												\
		loopFdWrap = prevFdWrap -> next;								\
	}																	\
	else {																\
		head = loopFdWrap -> next;										\
		free(loopFdWrap);												\
		loopFdWrap = head;												\
	}
/* signal */
typedef void (*sighandler_t)(int);

/* Function prototypes */           				
sighandler_t Signal(int signum, sighandler_t handler); 
int close_socket(int sock);

#ifdef DEBUG
void sigint_handler(int sig) {
	/* do nothing, just test the performance when being interrupted */
    return;
}
#endif


struct fdWrap {
	int fd;
	int bufSize;
	char buf[BUF_SIZE];
	struct fdWrap * next;
} * readHead, * writeHead;

int main(int argc, char* argv[])
{
    int sock, client_sock;
    ssize_t readret, writeret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    fd_set readset, writeset, exceptset;
    struct timeval timeout;
    int nfds = 0;
    int selectRet;
  	/* readValid, writeValid are flags to mark when to release resource */
    fd_set readValid, writeValid;
    struct fdWrap * tempFdWrap, * loopFdWrap, * prevFdWrap;
    
    fprintf(stdout, "----- Echo Server -----\n");	
    
	/* init process */
	readHead = NULL;
	writeHead = NULL;
	FD_ZERO(&readValid);
	FD_ZERO(&writeValid);
    
    /* ignore ctrl + c */
#ifndef DEBUG
    Signal(SIGINT, SIG_IGN);
#else
	Signal(SIGINT, sigint_handler);
#endif
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

	/* listen to the sock */
    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */
    
    /* not really needed currently, choose a random number*/
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    while (1)
    {
    	/* init fd sets */
        FD_ZERO(&readset);
    	FD_ZERO(&writeset);
    	FD_ZERO(&exceptset);
        /* add listen fd to set, but not to list
     	 * as listen fd should be treated differently from client fd
     	 */
     	nfds = sock;
    	FD_SET(sock, &readset);
    	/* if no write buf exists, add read fd in list to set;
    	 * add all write fd to set */ 
    	loopFdWrap = readHead;
    	while(loopFdWrap) {
    		if(!FD_ISSET(loopFdWrap -> fd, &writeValid)) {
    			nfds = MAX(nfds, loopFdWrap -> fd);
    			FD_SET(loopFdWrap -> fd, &readset); 
    		}
    		loopFdWrap = loopFdWrap -> next;	
    	} 	
    	loopFdWrap = writeHead;
    	while(loopFdWrap) {
    		nfds = MAX(nfds, loopFdWrap -> fd);
    		FD_SET(loopFdWrap -> fd, &writeset); 
    		loopFdWrap = loopFdWrap -> next;	
    	} 
    	/* nfds is the highest fd plus one */
    	nfds++;
    	/* begin select */
    	if((selectRet = select(nfds, &readset, &writeset, &exceptset, &timeout)) > 0) {
#ifdef DEBUG
    	    fprintf(stdout, "Selected fd(s): %d.\n", selectRet);
#endif
    		/* TODO: error handling, currently just ignore error from accept */
    	    if(FD_ISSET(sock, &readset)) {
    	    	/* establish new client socket */
    	    	cli_size = sizeof(cli_addr);
       			if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                		&cli_size)) != -1) {
       				/* add new client fd to read list */
       				if(!readHead) {
       					readHead = malloc(sizeof(struct fdWrap));
       					readHead -> next = NULL;
       				}
       				else {
       					tempFdWrap = readHead;
       					readHead = malloc(sizeof(struct fdWrap));
       					readHead -> next = tempFdWrap;
       				}      				
    				readHead -> fd = client_sock;
    				readHead -> bufSize = 0;
    				/* conn just established, refer equals 1 */
    				FD_SET(client_sock, &readValid);
#ifdef DEBUG
    	    		fprintf(stdout, "New Connection %d Established.\n", client_sock);
#endif
    			}
    		}
    		
    		/* search for available READ fd in read list 
    		 * only close socket in read loop, to prevent race, i.e., if close a socket
    		 * when writing, then a new connection arrives */
    		loopFdWrap = readHead;
    		while(loopFdWrap) {
    	    	if(!FD_ISSET(loopFdWrap -> fd, &readValid)) {
    	    		close_socket(loopFdWrap -> fd);
    				REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, readHead);
    				continue;
    			}
    				
    			if(FD_ISSET(loopFdWrap -> fd, &readset)) {
#ifdef DEBUG
    	    		fprintf(stdout, "Begin reading: %d.\n", loopFdWrap -> fd);
#endif
    				/* TODO: error handling, error type should be more specific */
    				if((readret = recv(loopFdWrap -> fd, loopFdWrap -> buf, 
    						BUF_SIZE, 0)) > 0) {
#ifdef DEBUG
    	    			fprintf(stdout, "Begin reading: %d.\n", loopFdWrap -> fd);
#endif
    					loopFdWrap -> bufSize = readret;
    					/* respond, i.e. keep it in write buf 
       					 * add new client fd to write list */
       					if(!writeHead) {
       						writeHead = malloc(sizeof(struct fdWrap));
       						writeHead -> next = NULL;
       					}
       					else {
       						tempFdWrap = writeHead;
       						writeHead = malloc(sizeof(struct fdWrap));
       						writeHead -> next = tempFdWrap;
       					}
       					/* return the same content */
       					memcpy(writeHead -> buf, loopFdWrap -> buf, loopFdWrap -> bufSize); 
       					writeHead -> bufSize = loopFdWrap -> bufSize;      				
    					writeHead -> fd = loopFdWrap -> fd;
    					/* a new write */
    					FD_SET(writeHead -> fd, &writeValid); 
    					
    					/* proceed next node */
    					loopFdWrap = loopFdWrap -> next;    					
										
    				}
    				else {
#ifdef DEBUG
    	    			fprintf(stdout, "recv failed: %s.\n", readret == -1? 
    	    				strerror(errno) : "peer shut down");
#endif
    					/* if interrupted, read next time; otherwise, release it;
    					 * or if recv returns 0, which means peer shut down the 
    					 * connection, close the socket */
    					if(readret == 0 || (readret == -1 && errno != EINTR)) {
    						FD_CLR(loopFdWrap -> fd, &readValid);
    						FD_CLR(loopFdWrap -> fd, &writeValid);
           					close_socket(loopFdWrap -> fd);
            				/* remove and free a write wrap from the linked list */
							REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, readHead);							         					
           				}
           				else
           					loopFdWrap = loopFdWrap -> next;
    				}
    			}
    			else {
    				prevFdWrap = loopFdWrap;
    				loopFdWrap = loopFdWrap -> next;
    			}
    		}
    		
    		/* search for available WRITE fd in write list */
    		loopFdWrap = writeHead;
    		prevFdWrap = NULL;
    		while(loopFdWrap) {
    			if(!FD_ISSET(loopFdWrap -> fd, &writeValid)) {
    				REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, writeHead);
    				continue;
    			}
    			if(FD_ISSET(loopFdWrap -> fd, &writeset)) {
#ifdef DEBUG
    	    		fprintf(stdout, "Begin writing.\n");
#endif 				  				
    				/* TODO: error handling, error type should be more specific */
           			if ((writeret = send(loopFdWrap -> fd, loopFdWrap -> buf, 
           				loopFdWrap -> bufSize, 0)) != loopFdWrap -> bufSize) {	
    				    /* if some bytes have been sent */
           				if(writeret != -1) {
           				    loopFdWrap -> bufSize -= writeret;
           					/* src and dest may overlap, so use memmove instead of memcpy */
           					memmove(loopFdWrap -> buf, (loopFdWrap -> buf + writeret), 
           						loopFdWrap -> bufSize);
           					loopFdWrap = loopFdWrap -> next;          			
           				}
           				else {
           					/* failure due to an interruption, send should restart; 
           				 	* otherwise, the connection is no longer effective, remove it;
           				 	*/
#ifdef DEBUG
    	    				fprintf(stdout, "write failed: %s.\n", strerror(errno));
#endif
           					if(errno != EINTR) {
           						FD_CLR(loopFdWrap -> fd, &readValid);
           						FD_CLR(loopFdWrap -> fd, &writeValid);
            					/* remove and free a write wrap from the linked list, close 
            					 * socket when in read loop */
								REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, writeHead);							       					
           					}
           					else
           						loopFdWrap = loopFdWrap -> next; 
           				}
           			}
           			else {
						FD_CLR(loopFdWrap -> fd, &writeValid);
						REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, writeHead);						
           			}
    			}
    			else {
    				prevFdWrap = loopFdWrap;
    				loopFdWrap = loopFdWrap -> next;
    			}
    		}
    	}
    	/* end select */
    }
	/* free all resources */
    close_socket(sock);

    return EXIT_SUCCESS;
}

int close_socket(int sock)
{
#ifdef DEBUG
   	fprintf(stdout, "Close socket %d.\n", sock);
#endif
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

sighandler_t Signal(int signum, sighandler_t handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); 
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0) {
    	fprintf(stderr, "Failed to setup signal handler: %s.\n",
    		strerror(errno));
    }
    return (old_action.sa_handler);
}
