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
#include <openssl/ssl.h>
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
#include "CGIHandler.h"
#include "requestHandler.h"
#include "lisod.h"
#include "common.h"

int main(int argc, char* argv[])
{
	int i, fileNameLen;
	int http_port, https_port;	
	char * log_file, * log_dir, * lock_file, * cgi_path, * ssl_key, * ssl_crt;
	int lock_fd;
    int client_sock;
    int responseSize;
    ssize_t readlen, readret, writeret;
    socklen_t cli_size;
    struct sockaddr_in cli_addr;
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
    SSL *client_context;
    int pipeInFd, pipeOutFd;
    
    fprintf(stdout, "----- Echo Server -----\n");	
    
    /* init process */
	readHead = NULL;
	writeHead = NULL;
	FD_ZERO(&readValid);
	FD_ZERO(&writeValid);
	http_sock = 0;
	https_sock = 0;
	log_fd = 0;
    
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
    	lock_file = argv[4];
    	cgi_path = argv[6];
    	ssl_key = argv[7];
    	ssl_crt = argv[8];
    	/* get www root */
    	www_root_len = strlen(argv[5]);
    	if(www_root_len > SMALL_BUF_SIZE)
    		error_exit("The length of www root is not long!");
    	memcpy(_www_root, argv[5], www_root_len);
    }

#ifndef DEBUG    
    /* daemonizing */
    daemonize(lock_file);    
    
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
    
    /* open log file */
    if((log_fd = open(log_file, (O_RDWR | O_CREAT))) < 0) 
    	error_exit("Cannot open log file.");
#endif
    	
    /* log daemonizing */
    sprintf(buf, "Successfully daemonized lisod process, pid %d.", getpid());
	msg_log("Log", buf);
    
    /* ignore ctrl + c, if DEBUG, use ctrl+c to test interruption */
#ifndef DEBUG
    Signal(SIGINT, SIG_IGN);
#else
	Signal(SIGINT, sigint_handler);
#endif

	/* initialize http */
	mHTTP_init(http_port);
	mSSL_init(https_port, ssl_key, ssl_crt);
	

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
    		/* HTTP: Accept connection, use wrapper func to log errors */
    	    if(FD_ISSET(http_sock, &readset)) {
    	    	/* establish new client socket */
    	    	cli_size = sizeof(cli_addr);
				if ((client_sock = Accept(http_sock, 
					(struct sockaddr *) &cli_addr, &cli_size)) != -1) {					
       				/* add new client fd to read list */
       				ADD_LINKEDLIST_NODE(readHead, client_sock);
    				readHead -> bufSize = 0;
    				readHead -> prot = HTTP;
    				/* set corresponding read valid as 1 */
    				FD_SET(client_sock, &readValid);
    			}
    		}
    		/* HTTPS: Accept connection, use wrapper func to log errors */
    		if(FD_ISSET(https_sock, &readset)) {
    	    	/* establish new client socket */
    	    	cli_size = sizeof(cli_addr);
				if ((client_sock = Accept(https_sock, 
					(struct sockaddr *) &cli_addr, &cli_size)) != -1) {
										
					/************ WRAP SOCKET WITH SSL ************/
					if (((client_context = SSL_New(ssl_context)) != NULL) &&
						SSL_Set_fd(client_context, client_sock) &&
						SSL_Accept(client_context)) {
					/************ END WRAP SOCKET WITH SSL ************/		
					
		   				/* add new client fd to read list */
		   				ADD_LINKEDLIST_NODE(readHead, client_sock);
		   				readHead -> ssl_fd = client_context;
						readHead -> bufSize = 0;
						readHead -> prot = HTTPS;
						/* set corresponding read valid as 1 */
						FD_SET(client_sock, &readValid);
    				}
    			}
    		}
    		
    		/* search for available READ fd in read list 
    		 * only close socket in read loop, to prevent race, i.e., 
    		 * if close a socket when writing, then a new connection 
    		 * arrives */
    		loopFdWrap = readHead;
    		while(loopFdWrap) {
    	    	if(!FD_ISSET(loopFdWrap -> fd, &readValid)) {
    	    		mClose_socket(loopFdWrap);
    				REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, readHead);
    				continue;
    			}
    				
    			if(FD_ISSET(loopFdWrap -> fd, &readset)) {
    				/* TODO: more specific error handling */
    				readlen = BUF_SIZE - loopFdWrap -> bufSize;
    				if((readret = mRecv(loopFdWrap -> fd, loopFdWrap -> ssl_fd,
    					buf, readlen, loopFdWrap -> prot)) > 0) {
    					/* append the bytes to loop's buf */
    					memcpy(loopFdWrap -> buf + loopFdWrap -> bufSize, 
    						buf, readret);
    					loopFdWrap -> bufSize += readret;    					
       					responseSize = HandleHTTP(loopFdWrap -> buf,
       							&loopFdWrap -> bufSize, buf, loopFdWrap -> fd);
    					if(responseSize > 0) {
    						/* request processed; then respond, 
							 * i.e. keep it in write buf and
		   					 * add new client fd to write list */
		   					ADD_LINKEDLIST_NODE(writeHead, 
		   						loopFdWrap -> fd);
		   					writeHead -> ssl_fd = loopFdWrap -> ssl_fd;
		   					writeHead -> prot = loopFdWrap -> prot;
		   					writeHead -> toClose = _close_conn;
							/* a new write */
		   					FD_SET(writeHead -> fd, &writeValid);
    						if(!_is_CGI) {
    							writeHead -> isCGI = 0;
		   						memcpy(writeHead -> buf, buf, responseSize);
								writeHead -> bufSize = responseSize;
								/* for big files, write the remaining 
								 * when can write */
								writeHead -> has_remain = _has_remain_bytes;
								if(_has_remain_bytes) {
									writeHead -> remain_bytes = _remain_bytes;
									writeHead -> offset = _file_offset;
									strcpy(writeHead -> path, _www_path);
								}		
								loopFdWrap -> bufSize = 0;
    						}
    						else {
    							CreatePipe(cgi_path, &pipeInFd, &pipeOutFd);
    							writeHead -> pipeFd = pipeOutFd;
    							writeHead -> isCGI = 1;
    							if(responseSize > loopFdWrap -> bufSize) {
    								loopFdWrap -> has_remain = 1;
    								loopFdWrap -> remain_bytes = responseSize
    									- loopFdWrap -> bufSize;
    								WriteToPipe(pipeInFd, buf, 
    									loopFdWrap -> bufSize);
    								loopFdWrap -> bufSize = 0;
    								loopFdWrap -> isCGI = 1;
    								loopFdWrap -> pipeFd = pipeInFd;
    							}
    							else {
    								loopFdWrap -> has_remain = 0;
    								WriteToPipe(pipeInFd, buf, responseSize);
    								loopFdWrap -> bufSize -= responseSize;
    								if(loopFdWrap -> bufSize > 0) {
    									memmove(loopFdWrap -> buf, 
    										loopFdWrap -> buf + responseSize,
    										loopFdWrap -> bufSize);
    								}
    								close_file(pipeInFd);
    							}
    						}
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
           					mClose_socket(loopFdWrap);
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
					if(loopFdWrap -> isCGI) {
						/* load content from pipe */
						loopFdWrap -> bufSize = ReadFromPipe(
							loopFdWrap -> pipeFd, loopFdWrap -> buf, BUF_SIZE);
						if(loopFdWrap -> bufSize == 0)
							continue;
					} 				
    				/* TODO: more specific error handling */
					if ((writeret = mSend(loopFdWrap -> fd, loopFdWrap->ssl_fd,
						loopFdWrap -> buf, loopFdWrap -> bufSize, 
						loopFdWrap -> prot)) != loopFdWrap -> bufSize) {
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
           						/* send a fin to notify the client, if error */
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
           					/* send a fin to notify the client, 
           					 * only needed for HTTP/1.0 */
           					if(loopFdWrap -> toClose)
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

int mClose_socket(struct fdWrap * wrap) {
	int ret;
	close_socket(wrap -> fd);
	if(wrap -> prot == HTTPS) {
	/*  
	  0 The shutdown is not yet finished. Call SSL_shutdown() for a second
    	time, if a bidirectional shutdown shall be performed.
      1 The shutdown was successfully completed. The "close notify" alert
        was sent and the peer's "close notify" alert was received.
     -1 The shutdown was not successful because a fatal error occurred
        either at the protocol level or a connection failure occurred. */
		ret = SSL_shutdown(wrap -> ssl_fd);
		switch(ret) {
			case 1:
				break;
			case 0:
				SSL_shutdown(wrap -> ssl_fd);
				break;
			case -1:
				error_log("SSL shut down error!");
				break;
		}
		SSL_free(wrap -> ssl_fd);
	}
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
	error_log(msg);
	liso_shutdown();
}

void liso_shutdown() {
	if(ssl_context != NULL)
		SSL_CTX_free(ssl_context);
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

void signal_handler(int sig) {
	switch(sig)
	{
		case SIGHUP:
				/* rehash the server */
				break;          
		case SIGTERM:
				/* finalize and shutdown the server */
				liso_shutdown();
				break;    
		default:
				break;
				/* unhandled signal */      
	} 
}

int daemonize(char* lock_file) {
	int i, pid, lock_fd;
	char pid_str[TINY_BUF_SIZE];
    /* daemonizing */
    pid = fork();
    if (pid < 0) 
    	exit(EXIT_FAILURE); /* fork error */
    if (pid > 0) 
    	exit(EXIT_SUCCESS); /* parent exits */
	/* child (daemon) continues */
    /* close all fds inherited */
    for (i = getdtablesize(); i >= 0; --i)
		close(i);
	/* dup stdout & stderr to /dev/null */
    i = open("/dev/null", O_RDWR); /* open stdin */
    dup(i); /* stdout */
    dup(i); /* stderr */
    /* protects the files created */
    umask(027);
    /* lock the lock file to prevent multiple instances */
    lock_fd = open(lock_file, O_RDWR|O_CREAT, 0640);
	if (lock_fd < 0)
		exit(EXIT_FAILURE); /* cannot open */
	if (lockf(lock_fd, F_TLOCK, 0) < 0)
		exit(EXIT_SUCCESS); /* cannot lock */
	/* only first instance continues */
	sprintf(pid_str, "%d\n", getpid());
	write(lock_fd, pid_str, strlen(pid_str)); /* record pid to lockfile */
	
	Signal(SIGCHLD, SIG_IGN); /* child terminate signal */
    Signal(SIGHUP, signal_handler); /* hangup signal */
    Signal(SIGTERM, signal_handler); /* software termination sig from kill */

	return EXIT_SUCCESS;
}

void mHTTP_init(int http_port) {
	struct sockaddr_in http_addr;
    /* Create HTTP socket */
    if ((http_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        error_exit("Failed creating HTTP socket.");	
	/* bind socket */
    http_addr.sin_family = AF_INET;
    http_addr.sin_port = htons(http_port);
    http_addr.sin_addr.s_addr = INADDR_ANY;
    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(http_sock, (struct sockaddr *) &http_addr, sizeof(http_addr)))
    	error_exit("Failed binding HTTP socket.");   
	/* listen to the sock */
    if (listen(http_sock, 50)) 
    	error_exit("Error listening on HTTP socket.");
}

/* Wrapper functions that init all SSL settings */
void mSSL_init(int https_port, char * key, char * crt) {
    struct sockaddr_in addr;
    /************ SSL INIT ************/
    SSL_load_error_strings();
    SSL_library_init();

    /* we want to use TLSv1 only */
    if ((ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
    {
        error_exit("Error creating SSL context.\n");
    }

    /* register private key */
    if (SSL_CTX_use_PrivateKey_file(ssl_context, key, SSL_FILETYPE_PEM) == 0)
    {
        error_exit("Error associating private key.\n");
    }

    /* register public key (certificate) */
    if (SSL_CTX_use_certificate_file(ssl_context, crt, SSL_FILETYPE_PEM) == 0)
    {
        error_exit("Error associating certificate.\n");
    }
    /************ END SSL INIT ************/

    /************ SERVER SOCKET SETUP ************/
    if ((https_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_exit("Failed creating HTTPS socket.\n");
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(https_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(https_sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        error_exit("Failed binding HTTPS socket.\n");
    }

    if (listen(https_sock, 50))
    {
       	error_exit("Error listening on HTTPS socket.\n");
    }
    /************ END SERVER SOCKET SETUP ************/
}


/* Wrapper functions */
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
    int rc;
    char error_msg[TINY_BUF_SIZE];
    
    if ((rc = accept(s, addr, addrlen)) < 0) {
		sprintf(error_msg, "%s: %s\n", "Accept Failed", strerror(errno));
		error_log(error_msg);
	}
    return rc;
}

SSL *SSL_New(SSL_CTX * ssl_context) {
	SSL *client_context;
	if ((client_context = SSL_new(ssl_context)) == NULL)
	{
		error_log("Error creating client SSL context.\n");
	}
	return client_context;
}

int SSL_Set_fd(SSL * client_context, int client_sock) {
	if (SSL_set_fd(client_context, client_sock) == 0)
	{
		SSL_free(client_context);
		error_log("Error creating client SSL context.\n");
		return 0;
	}
	return 1;
}

int SSL_Accept(SSL * client_context) {
	if (SSL_accept(client_context) <= 0)
	{
		SSL_free(client_context);
		error_log("Error accepting (handshake) client SSL context.\n");
		return 0;
	}
	return 1;
}

ssize_t mRecv(int sockfd, SSL * ssl_fd, void *buf, size_t readlen, 
	enum protocol prot) {
	int readret;
	if(prot == HTTP) {
		if((readret = recv(sockfd, buf, readlen, 0)) < 0) {
			error_log("Recv error!\n");
		}
	}
	else {
		if((readret = SSL_read(ssl_fd, buf, readlen)) < 0) {
			error_log("SSL read error!\n");
		}
	}
	return readret;
}

ssize_t mSend(int sockfd, SSL * ssl_fd, const void * buf, size_t writelen, 
	enum protocol prot) {
	int writeret;
	if(prot == HTTP) {
		if((writeret = send(sockfd, buf, writelen, 0)) < 0) {
			error_log("Send error!\n");
		}
	}
	else {
		if((writeret = SSL_write(ssl_fd, buf, writelen)) < 0) {
			error_log("SSL write error!\n");
		}
		
	}
	return writeret;
}
