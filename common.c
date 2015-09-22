#include "common.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void msg_log(char * token, char * msg) {
	fprintf(stdout, "%s: %s\n", token, msg);
}

void error_log(char * msg) {
	fprintf(stderr, "ERROR: %s\r\n", msg);
}

int close_file(int fd) {
    if (close(fd)) {
        error_log("Failed closing file.");
        return 1;
    }
    return 0;
}

int open_file(const char *pathname, int flags) {
	int fd;
	if((fd = open(pathname, flags)) < 0)
		error_log("Failed opening file.");
	return fd;
}

int close_log_file(int fd) {
	/* close log file and redirect output to stdout & stderr */
	dup2(_saved_stderr, STDERR_FILENO);
	dup2(_saved_stdout, STDOUT_FILENO);
	return close_file(fd);
}
