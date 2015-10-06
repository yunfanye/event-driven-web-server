#include "common.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

void msg_log(char * token, char * msg) {
	char buf[SMALL_BUF_SIZE];
	/* when log file has been not open, print to stdout */
	if(log_fd) {
		sprintf(buf, "%s: %s\n", token, msg);
		write(log_fd, buf, strlen(buf));
	}
	else
		printf("%s: %s\n", token, msg);
}

void error_log(char * msg) {
	msg_log("ERROR", msg);
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
	return close_file(fd);
}
