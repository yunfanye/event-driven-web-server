#include "common.h"

#ifndef _REQUEST_HANDLER
#define _REQUEST_HANDLER

/* function prototypes */
int HandleHTTP(char * buf, int * buf_size, char * out_buf, int socket);
int get_response(char * out_buf, int closeConn);
int get_message(enum status code, char * msg);
off_t get_body(int fd, char * out_buf, off_t size);

struct type_map {
	char surfix[TINY_BUF_SIZE];
	char type[TINY_BUF_SIZE];
};

enum parse_state {
	STATE_START = 0,
	STATE_CR,
	STATE_CRLF,
	STATE_CRLFCR,
	STATE_CRLFCRLF
};

/* external reference from lisod.c, set by command line */
extern char _www_root[SMALL_BUF_SIZE];
/* external refernce from yacc parser */
extern void set_parsing_buf(char *buf, size_t siz);

/* global variables that are referenced in yacc */
char _token[SMALL_BUF_SIZE];
char _text[SMALL_BUF_SIZE];

/* global variables that are referenced in lisod.c */
char _www_path[SMALL_BUF_SIZE];
char _has_remain_bytes;
off_t _remain_bytes;
off_t _file_offset;
int _is_CGI;

/* global variables */
static char had_request_line;
static char _method[SMALL_BUF_SIZE];
static char _uri[SMALL_BUF_SIZE];
static char _prot[SMALL_BUF_SIZE];
static enum status code;
static char status_message[TINY_BUF_SIZE];	


#endif
