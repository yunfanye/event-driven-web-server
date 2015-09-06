/****************************************************************************
* echo_server.c																*
* 																		 	*
* Description: This file built on the provided starter code. contains the C	*
*			   source code for an echo server. The server runs on ports		*
*			   specified by command line arguments and simply write back 	*
*			   anything sent to it by connected clients. It supports 		*
* 			   concurrent clients by an event-drivin model, i.e. "select"	*
*			   function in c.             									*
*                                                                           *
* Author: Yunfan Ye <yunfany@andrew.cmu.edu>,                         		*
*																			*
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
#include <sys/stat.h>
#include <fcntl.h>


/* uncomment the following line to debug */
/* #define DEBUG */

/* Macros */
/* the number of available file descriptors is typically 1024 */

/* if bytes sent exceed BUF_SIZE, then it will proceed it by several reads */
#define BUF_SIZE 40960

#define MAX(x, y)  ((x) > (y) ? (x) : (y))
/* only C99 support inline function, so just use macros */
/* remove and free a wrap node from the linked list
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

#define ADD_LINKEDLIST_NODE(head, client_fd)							\
	if(!head) {															\
		head = malloc(sizeof(struct fdWrap));							\
		head -> next = NULL;											\
	}																	\
	else {																\
		tempFdWrap = head;												\
		head = malloc(sizeof(struct fdWrap));							\
		head -> next = tempFdWrap;										\
	}																	\
	head -> fd = client_fd;

/* signal */
typedef void (*sighandler_t)(int);

/* Function prototypes */           				
sighandler_t Signal(int signum, sighandler_t handler); 
int close_socket(int sock);
void error_exit(char * msg);

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

/* global variables */
int http_sock, https_sock;
int log_fd;

int main(int argc, char* argv[])
{
	int http_port, https_port;	
	char * log_file;
    int client_sock;
    ssize_t readret, writeret;
    socklen_t cli_size;
    struct sockaddr_in http_addr, https_addr, cli_addr;
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
	http_sock = 0;
	https_sock = 0;
	log_fd = 0;
	
	/* ignore ctrl + c, if DEBUG, use ctrl+c to test interruption */
#ifndef DEBUG
    Signal(SIGINT, SIG_IGN);
#else
	Signal(SIGINT, sigint_handler);
#endif
    
    /* read the command line, init config */
    if(argc < 9) {
    	/* not enought command line arguments */
		error_exit("Not enought command line arguments!");
    }
    else {
    	/* since only http port is used currently, other parameters 
    	 * are simply ignored */
    	http_port = atoi(argv[1]);
    	https_port = atoi(argv[2]);
    	/* check the input, beyond the range, the port is invalid */
    	if(http_port > 65535 || http_port < 0)
    		error_exit("HTTP port number invalid!");
    	if(https_port > 65535 || https_port < 0) 
    		error_exit("HTTPS port number invalid!");
    	if(https_port == http_port)
    		error_exit("HTTP port number should not equal to that of HTTPS");
    	/* get log file */
    	log_file = argv[3];
    }
        
    /* if log file exists, append string; if not create it; we don't check
     * whether the directory exists, as the server manager should make a
     * decision */
    if((log_fd = open(log_file, (O_WRONLY | O_CREAT))) < 0) 
    	error_exit("Cannot open log file.");
    /* Redirect stderr to stdout (so that we will get all output
     * on the pipe connected to stdout) */
    if(dup2(1, 2) < 0)
    	error_exit("Cannot redirect stderr to stdout");
    /* redirect stdout to log_file */
    if (dup2(log_fd, STDOUT_FILENO) < 0)
    	error_exit("Cannot redirect stdout to log file");

    /* Create HTTP socket */
    if ((http_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        error_exit("Failed creating HTTP socket.");
    /* Create HTTPS socket */
    if ((https_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        error_exit("Failed creating HTTPS socket.");
	
	/* bind socket */
    http_addr.sin_family = AF_INET;
    http_addr.sin_port = htons((short)http_port);
    http_addr.sin_addr.s_addr = INADDR_ANY;
    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(http_sock, (struct sockaddr *) &http_addr, sizeof(http_addr)))
    	error_exit("Failed binding HTTP socket.");
    	
    https_addr.sin_family = AF_INET;
    https_addr.sin_port = htons((short)https_port);
    https_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(https_sock, (struct sockaddr *) &https_addr, sizeof(https_addr)))
    	error_exit("Failed binding HTTPS socket.");
    
	/* listen to the sock */
    if (listen(http_sock, 500)) 
    	error_exit("Error listening on HTTP socket.");
    if (listen(https_sock, 500)) 
    	error_exit("Error listening on HTTPS socket.");

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
        /*
         * add listen fd to set, but not to list
     	 * as listen fd should be treated differently from client fd
     	 */
    	FD_SET(http_sock, &readset);   	
    	/* add https fd */    	
    	FD_SET(https_sock, &readset);
    	nfds = MAX(http_sock, https_sock);
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
    	if((selectRet = select(nfds, &readset, &writeset, &exceptset, 
    		&timeout)) > 0) {

    		/* TODO: error handling, currently just ignore error from accept */
    	    if(FD_ISSET(http_sock, &readset)) {
    	    	/* establish new client socket */
    	    	cli_size = sizeof(cli_addr);
				if ((client_sock = accept(http_sock, (struct sockaddr *) &cli_addr,
                		&cli_size)) != -1) {
       				/* add new client fd to read list */
       				ADD_LINKEDLIST_NODE(readHead, client_sock);
    				readHead -> bufSize = 0;
    				/* set corresponding read valid as 1 */
    				FD_SET(client_sock, &readValid);
    			}
    		}
    		/* do not distinguish between http & https for current service */
    		if(FD_ISSET(https_sock, &readset)) {
    	    	/* establish new client socket */
    	    	cli_size = sizeof(cli_addr);
				if ((client_sock = accept(https_sock, (struct sockaddr *) &cli_addr,
                		&cli_size)) != -1) {
       				/* add new client fd to read list */
       				ADD_LINKEDLIST_NODE(readHead, client_sock);
    				readHead -> bufSize = 0;
    				/* set corresponding read valid as 1 */
    				FD_SET(client_sock, &readValid);
    			}
    		}
    		
    		/* search for available READ fd in read list 
    		 * only close socket in read loop, to prevent race, i.e., 
    		 * if close a socket when writing, then a new connection 
    		 * arrives */
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
    				/* TODO: more specific error handling */
    				if((readret = recv(loopFdWrap -> fd, loopFdWrap -> buf, 
    						BUF_SIZE, 0)) > 0) {
#ifdef DEBUG
    	    			fprintf(stdout, "Begin reading: %d.\n", 
    	    				loopFdWrap -> fd);
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
       					memcpy(writeHead -> buf, loopFdWrap -> buf, 
       						loopFdWrap -> bufSize); 
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
    					/* if interrupted, read next time; otherwise, release 
						 * it. Or if recv returns 0, which means peer shut down
						 * the connection, close the socket */
    					if(readret == 0 || (readret == -1 && errno != EINTR)) {
    						FD_CLR(loopFdWrap -> fd, &readValid);
    						FD_CLR(loopFdWrap -> fd, &writeValid);
           					close_socket(loopFdWrap -> fd);
            				/* remove and free a write wrap from the linked 
            				 * list */
							REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, 
								readHead);		
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
    				/* TODO: more specific error handling */
					if ((writeret = send(loopFdWrap -> fd, loopFdWrap -> buf, 
						loopFdWrap -> bufSize, 0)) != loopFdWrap -> bufSize) {
    				    /* if some bytes have been sent */
           				if(writeret != -1) {
           				    loopFdWrap -> bufSize -= writeret;
							/* src and dest may overlap, so use memmove instead
           					 * of memcpy */
           					memmove(loopFdWrap -> buf, (loopFdWrap -> buf + 
           						writeret), loopFdWrap -> bufSize);
           					loopFdWrap = loopFdWrap -> next; 
           				}
           				else {
           					/* failure due to an interruption, send should
							 * restart; otherwise, the connection is no longer
           					 * effective, remove it;
           				 	 */
#ifdef DEBUG
    	    				fprintf(stdout, "write failed: %s.\n", 
    	    					strerror(errno));
#endif
           					if(errno != EINTR) {
           						FD_CLR(loopFdWrap -> fd, &readValid);
           						FD_CLR(loopFdWrap -> fd, &writeValid);
            					/* remove and free a write wrap from the linked
            					 * list, close socket when in read loop */
								REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap,
									writeHead);
           					}
           					else
           						loopFdWrap = loopFdWrap -> next; 
           				}
           			}
           			else {
						FD_CLR(loopFdWrap -> fd, &writeValid);
						REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, 
							writeHead);						
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
	close(log_fd);
    close_socket(http_sock);
    close_socket(https_sock);

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

/* Wrapper functions */
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

void error_exit(char * msg) {
	fprintf(stderr, "%s\n", msg);
	if(http_sock > 0)
		close_socket(http_sock);
	if(https_sock > 0)
		close_socket(https_sock);
	if(log_fd > 0)
		close(log_fd);
	exit(EXIT_FAILURE);
}

