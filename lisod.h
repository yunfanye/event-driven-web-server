#ifndef _LISOD
#define _LISOD

/* uncomment the following line to debug */
#define DEBUG

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

enum protocol {
	HTTP,
	HTTPS
};

struct fdWrap {
	int fd;
	int bufSize;
	char buf[BUF_SIZE];
	enum protocol prot;
	struct fdWrap * next;
	/* the following only used in write */
	char has_remain;	
	off_t remain_bytes;
	off_t offset;
	char path[SMALL_BUF_SIZE];
} * readHead, * writeHead;

/* global variables */
int http_sock, https_sock;
int log_fd;

char _www_root[SMALL_BUF_SIZE];

extern char _www_path[SMALL_BUF_SIZE];
extern char _has_remain_bytes;
extern off_t _remain_bytes;
extern off_t _file_offset;

#endif
