/****************************************************************************
* CGIHandler.c																*
* 																		 	*
* Description: Helper functions for CGI										*
*                                                                           *
* Author: Yunfan Ye <yunfany@andrew.cmu.edu>,                         		*
*																			*
*****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "CGIHandler.h"

int CreatePipe(char * cgi_path, int * pipeInFd, int * pipeOutFd, char * envp[]) {
    /*************** BEGIN VARIABLE DECLARATIONS **************/
    pid_t pid;
    int stdin_pipe[2];
    int stdout_pipe[2];
	char* argv[2];
	argv[0] = cgi_path;
	argv[1] = NULL;
    /*************** END VARIABLE DECLARATIONS **************/

    /*************** BEGIN PIPE **************/
    /* fd[0] can be read from, fd[1] can be written to */
    if (pipe(stdin_pipe) < 0)
    {
        error_log("Error piping for stdin.");
        return 0;
    }

    if (pipe(stdout_pipe) < 0)
    {
        error_log("Error piping for stdout.");
        return 0;
    }
    /*************** END PIPE **************/

    /*************** BEGIN FORK **************/
    pid = fork();
    /* not good */
    if (pid < 0)
    {
        error_log("Something really bad happened when fork()ing.");
        return 0;
    }

    /* child, setup environment, execve */
    if (pid == 0)
    {
        /*************** BEGIN EXECVE ****************/
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        dup2(stdout_pipe[1], fileno(stdout));
        dup2(stdin_pipe[0], fileno(stdin));
        /* you should probably do something with stderr */
        /* pretty much no matter what, if it returns bad things happened... */
        if (execve(cgi_path, argv, envp))
        {
        	execve_error_handler();
            error_log("Error executing execve syscall.\n");
            return 0;
        }
        /*************** END EXECVE ****************/ 
    }

    if (pid > 0)
    {
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);
		*pipeInFd = stdin_pipe[1];
		*pipeOutFd = stdout_pipe[0];
		return 1;
    }
    /*************** END FORK **************/
}
int WriteToPipe(int fd, char * buf, int size) {
    int nleft = size;
    int writeret;
    while (nleft > 0) {
		if ((writeret = write(fd, buf, nleft)) <= 0) {
			if (errno == EINTR)  /* Interrupted by sig handler return */
				writeret = 0;    /* and call write() again */
			else
				return -1;
		}
		nleft -= writeret;
		buf += writeret;
    }
    return size;
}
int ReadFromPipe(int fd, char * buf, int size) {
    int nleft = size;
    int readret;
    while (nleft > 0) {
		if ((readret = read(fd, buf, nleft)) < 0) {
			if (errno == EINTR) /* Interrupted by sig handler return */
				readret = 0;      /* and call read() again */
			else
				return -1;      /* errno set by read() */ 
		} 
		else if (readret == 0)
			break;              /* EOF */
		nleft -= readret;
		buf += readret;
    }
    return (size - nleft);         /* return >= 0 */
}


/**************** BEGIN UTILITY FUNCTIONS ***************/
/* error messages stolen from: http://linux.die.net/man/2/execve */
void execve_error_handler()
{
    switch (errno)
    {
        case E2BIG:
            fprintf(stderr, "The total number of bytes in the environment \
(envp) and argument list (argv) is too large.\n");
            return;
        case EACCES:
            fprintf(stderr, "Execute permission is denied for the file or a \
script or ELF interpreter.\n");
            return;
        case EFAULT:
            fprintf(stderr, "filename points outside your accessible address \
space.\n");
            return;
        case EINVAL:
            fprintf(stderr, "An ELF executable had more than one PT_INTERP \
segment (i.e., tried to name more than one \
interpreter).\n");
            return;
        case EIO:
            fprintf(stderr, "An I/O error occurred.\n");
            return;
        case EISDIR:
            fprintf(stderr, "An ELF interpreter was a directory.\n");
            return;
        case ELIBBAD:
            fprintf(stderr, "An ELF interpreter was not in a recognised \
format.\n");
            return;
        case ELOOP:
            fprintf(stderr, "Too many symbolic links were encountered in \
resolving filename or the name of a script \
or ELF interpreter.\n");
            return;
        case EMFILE:
            fprintf(stderr, "The process has the maximum number of files \
open.\n");
            return;
        case ENAMETOOLONG:
            fprintf(stderr, "filename is too long.\n");
            return;
        case ENFILE:
            fprintf(stderr, "The system limit on the total number of open \
files has been reached.\n");
            return;
        case ENOENT:
            fprintf(stderr, "The file filename or a script or ELF interpreter \
does not exist, or a shared library needed for \
file or interpreter cannot be found.\n");
            return;
        case ENOEXEC:
            fprintf(stderr, "An executable is not in a recognised format, is \
for the wrong architecture, or has some other \
format error that means it cannot be \
executed.\n");
            return;
        case ENOMEM:
            fprintf(stderr, "Insufficient kernel memory was available.\n");
            return;
        case ENOTDIR:
            fprintf(stderr, "A component of the path prefix of filename or a \
script or ELF interpreter is not a directory.\n");
            return;
        case EPERM:
            fprintf(stderr, "The file system is mounted nosuid, the user is \
not the superuser, and the file has an SUID or \
SGID bit set.\n");
            return;
        case ETXTBSY:
            fprintf(stderr, "Executable was open for writing by one or more \
processes.\n");
            return;
        default:
            fprintf(stderr, "Unkown error occurred with execve().\n");
            return;
    }
}
/**************** END UTILITY FUNCTIONS ***************/

