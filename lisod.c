/****************************************************************************
* lisod.c																	*
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
#include "requestHandler.h"
#include "lisod.h"
#include "common.h"

int main(int argc, char* argv[])
{
	int i, fileNameLen;
	int http_port, https_port;	
	char * log_file, * log_dir;
    int client_sock;
    int responseSize;
    ssize_t readlen, readret, writeret;
    socklen_t cli_size;
    struct sockaddr_in http_addr, https_addr, cli_addr;
    fd_set readset, writeset, exceptset;
    struct timeval timeout;
    int nfds = 0;
    int selectRet;
  	/* readValid, writeValid are flags to mark when to release resource */
    fd_set readValid, writeValid;
    struct fdWrap * tempFdWrap, * loopFdWrap, * prevFdWrap;
    char buf[BUF_SIZE];
    int www_root_len;
    char reload_success;
    int write_remain_fd, read_remain_len;
    
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
    	/* get www root */
    	www_root_len = strlen(argv[5]);
    	if(www_root_len > SMALL_BUF_SIZE)
    		error_exit("The length of www root is not long!");
    	memcpy(_www_root, argv[5], www_root_len);
    }
        
    /* if log file exists, append string; if not create it; if the 
     * directory doesn't exit, create it; That directly call mkdir()
     * saves a stat() */
    fileNameLen = strlen(log_file);
    for (i = fileNameLen - 2; i > 0; i--) {
    	if(log_file[i] == '/') {
    		log_dir = malloc(i + 1);
    		strncpy(log_dir, log_file, i + 1);
    		mkdir(log_dir, 0700);
    		free(log_dir);
    	}
    }
    
    if((log_fd = open(log_file, (O_RDWR | O_CREAT))) < 0) 
    	error_exit("Cannot open log file.");
    /* Save stderr, and redirect stderr to log file */
    if((_saved_stderr = dup(STDERR_FILENO)) < 0)
    	error_exit("Cannot save stderr");
    if(dup2(log_fd, STDERR_FILENO) < 0)
    	error_exit("Cannot redirect stderr to log file");
    /* Save stdout, and redirect stdout to log_file */
    if((_saved_stdout = dup(STDOUT_FILENO)) < 0)
    	error_exit("Cannot save stdout");
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
    http_addr.sin_port = htons(http_port);
    http_addr.sin_addr.s_addr = INADDR_ANY;
    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(http_sock, (struct sockaddr *) &http_addr, sizeof(http_addr)))
    	error_exit("Failed binding HTTP socket.");
    	
    https_addr.sin_family = AF_INET;
    https_addr.sin_port = htons(https_port);
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
    	/* if no corresponding write buf exists, add read fd in list to set;
    	 * Thus, for a specific socket, request must be handled one by one
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
				if ((client_sock = accept(http_sock, 
					(struct sockaddr *) &cli_addr, &cli_size)) != -1) {
       				/* add new client fd to read list */
       				ADD_LINKEDLIST_NODE(readHead, client_sock);
    				readHead -> bufSize = 0;
    				readHead -> prot = HTTP;
    				/* set corresponding read valid as 1 */
    				FD_SET(client_sock, &readValid);
    			}
    		}
    		/* do not distinguish between http & https for current service */
    		if(FD_ISSET(https_sock, &readset)) {
    	    	/* establish new client socket */
    	    	cli_size = sizeof(cli_addr);
				if ((client_sock = accept(https_sock, 
					(struct sockaddr *) &cli_addr, &cli_size)) != -1) {
       				/* add new client fd to read list */
       				ADD_LINKEDLIST_NODE(readHead, client_sock);
    				readHead -> bufSize = 0;
    				readHead -> prot = HTTPS;
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
    				/* TODO: more specific error handling */
    				readlen = BUF_SIZE - loopFdWrap -> bufSize;
    				if((readret = recv(loopFdWrap -> fd, buf, 
    						readlen, 0)) > 0) {
    					/* append the bytes to buf */
    					memcpy(loopFdWrap -> buf + loopFdWrap -> bufSize, 
    						buf, readret);
    					loopFdWrap -> bufSize += readret;
    					
    					/* proceed request and generate response */
					
       					if(loopFdWrap -> prot == HTTP)
       						responseSize = HandleHTTP(loopFdWrap -> buf,
       							&loopFdWrap -> bufSize, buf, loopFdWrap -> fd);
       					else
       						responseSize = HandleHTTPS(loopFdWrap -> buf,
       							loopFdWrap -> bufSize, buf);
    					if(responseSize > 0) {
    						/* request processed; then respond, 
    						 * i.e. keep it in write buf and
       						 * add new client fd to write list */
       						ADD_LINKEDLIST_NODE(writeHead, loopFdWrap -> fd);
       						memcpy(writeHead -> buf, buf, responseSize);
							writeHead -> bufSize = responseSize;
							/* for big files, write the remaining when write */
							writeHead -> has_remain = _has_remain_bytes;
							if(_has_remain_bytes) {
								writeHead -> remain_bytes = _remain_bytes;
								writeHead -> offset = _file_offset;
								strcpy(writeHead -> path, _www_path);
							}		
    						/* a new write */
    						FD_SET(writeHead -> fd, &writeValid);
    						loopFdWrap -> bufSize = 0;
    					}    					
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
           						/* send a fin to notify the client */
           						shutdown(loopFdWrap -> fd, SHUT_WR);
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
           				/* if there are remaining bytes, reload the buf;
           				 * here may cause an issue: file operation may fail,
           				 * while the header has been sent. Thus, even if 
           				 * error occurs, error code cannot be sent to client.
           				 * However, the FAQ has this requirement, i.e.
           				 * "Try to send a 'full buffer' of the file from disk
           				 * every time" */           				 
           				reload_success = 0;
           				if(loopFdWrap -> has_remain) {
           					/* has remaining bytes to send, reload the buf */
							if((write_remain_fd = open_file(loopFdWrap -> path,
								O_RDONLY)) >= 0) {
								/* move pointer to offset */
								lseek(write_remain_fd, loopFdWrap -> offset,
									SEEK_SET);
								read_remain_len = 
									MIN(loopFdWrap -> remain_bytes, BUF_SIZE);
								if(get_body(write_remain_fd, loopFdWrap -> buf, 
									read_remain_len) == read_remain_len) {
									reload_success = 1;
									loopFdWrap -> bufSize = read_remain_len;
									loopFdWrap -> offset += read_remain_len;
									loopFdWrap -> remain_bytes -= 
										read_remain_len;
									if(loopFdWrap -> remain_bytes == 0)
										loopFdWrap -> has_remain = 0;
								}
								/* close immediately to prevent being limited
								 * by slow client*/
								close_file(write_remain_fd);
							}													
           				}
           				if(reload_success)
           					loopFdWrap = loopFdWrap -> next; 
           				else {
           					/* send a fin to notify the client */
           					shutdown(loopFdWrap -> fd, SHUT_WR);
							FD_CLR(loopFdWrap -> fd, &writeValid);
							REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, 
								writeHead);	
						}					
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

#ifdef DEBUG
void sigint_handler(int sig) {
	/* do nothing, just test the performance when being interrupted -- cp1 */
	error_exit("SIG INT caught\r\n");
    return;
}
#endif

