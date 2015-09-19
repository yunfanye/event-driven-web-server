#include "common.h"

#ifndef _REQUEST_HANDLER
#define _REQUEST_HANDLER


int HandleHTTP(char * buf, int * buf_size, char * out_buf, int socket);
int HandleHTTPS(char * buf, int buf_size, char * out_buf);
int get_response(char * out_buf);
int get_message(enum status code, char * msg);
off_t get_body(int fd, char * out_buf, off_t size);


#endif
