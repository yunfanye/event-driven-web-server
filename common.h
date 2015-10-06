#ifndef _COMMON
#define _COMMON

#define MAX_HEADER_LEN 32768
#define TINY_BUF_SIZE 128
#define SMALL_BUF_SIZE 8192
/* if bytes sent exceed BUF_SIZE, then it will proceed it by several reads */
#define BUF_SIZE 40960
#define MAX(x, y)  ((x) > (y) ? (x) : (y))
#define MIN(x, y)  ((x) < (y) ? (x) : (y))

enum status {
    S_200_OK = 200,
    S_400_BAD_REQUEST = 400,
    S_403_FORBIDDEN = 403,
    S_404_NOT_FOUND = 404,
    S_411_LENGTH_REQUIRED = 411,
    S_500_INTERNAL_SERVER_ERROR = 500,
    S_501_NOT_IMPLEMENTED = 501,
    S_503_SERVICE_UNAVAILABLE = 503,
    S_505_HTTP_VERSION_NOT_SUPPORTED = 505
};

/* function prototypes */
void msg_log(char * token, char * msg);
void error_log(char * msg);
int open_file(const char *pathname, int flags);
int close_file(int fd);
int close_log_file(int fd);

/* external reference by lisod.c, log file descriptor */
int log_fd;

#endif
