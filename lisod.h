#ifndef _LISOD
#define _LISOD

/* uncomment the following line to debug */
/* #define DEBUG */

/* only C99 support inline function, so just use macros */
/* remove and free a wrap node from the linked list
 * prevFdWrap == NULL only if loopFdWrap == head */	
#define REMOVE_LINKEDLIST_NODE(loopFdWrap, prevFdWrap, head)			\
	if(loopFdWrap -> isCGI)												\
		close_file(loopFdWrap -> pipeFd);								\
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
	
/* Add new wrap node to the front of the list */
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

enum protocol {
	HTTP,
	HTTPS
};

struct fdWrap {
	int fd;
	SSL * ssl_fd;
	int bufSize;
	char buf[BUF_SIZE];
	enum protocol prot;
	struct fdWrap * next;
	/* has_remain - has remain bytes in a file to write OR
	 * has remain bytes to read from a socket */
	char has_remain;	
	off_t remain_bytes;
	off_t offset;
	char path[SMALL_BUF_SIZE];
	int isCGI;
	int pipeFd;
	/* if Connection: Close header is sent, toClose = 1 */
	int toClose;
} * readHead, * writeHead;

/* Function prototypes */           				
sighandler_t Signal(int signum, sighandler_t handler); 
int close_socket(int sock);
void error_exit(char * msg);
int daemonize(char* lock_file);
void liso_shutdown();
void mHTTP_init(int port);
void mSSL_init(int port, char * key, char * crt);
void free_strings(char ** envp);

/* Wrapper functions */
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen);
SSL *SSL_New(SSL_CTX *ctx);
int SSL_Set_fd(SSL *ssl, int fd);
int SSL_Accept(SSL *ssl);
ssize_t mRecv(int sockfd, SSL * ssl_fd, void *buf, size_t len,
	enum protocol prot);
ssize_t mSend(int sockfd, SSL * ssl_fd, const void *buf, size_t len,
	enum protocol prot);
int mClose_socket(struct fdWrap * wrap);

#ifdef DEBUG
void sigint_handler(int sig);
#endif

/* global variables */
static int http_sock, https_sock;
SSL_CTX *ssl_context;
static int lock_fd;

/* external reference by requestHandler.c */
char _www_root[SMALL_BUF_SIZE];

/* external referenced by CGIHandler.c */
int _http_port, _https_port;

/* external reference from requestHandler.c */
extern char _www_path[SMALL_BUF_SIZE];
extern char _has_remain_bytes;
extern off_t _remain_bytes;
extern off_t _file_offset;
extern int _is_CGI;
extern int _close_conn;
extern char * _envp[ENVP_SIZE];

/* external reference from common.h, log file descriptor */
extern int log_fd;

#endif
