#include "common.h"

#ifndef _CGI_HANDLER
#define _CGI_HANDLER

/* function prototypes */
int CreatePipe(char * cgi_path, int * pipeInFd, int * pipeOutFd, char ** envp);
int WriteToPipe(int fd, char * buf, int size);
int ReadFromPipe(int fd, char * buf, int size);
void execve_error_handler();

#endif
