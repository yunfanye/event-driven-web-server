#include "requestHandler.h"
#include <string.h>

int HandleHTTP(char * buf, int * bufSize, char * outBuf) {
	memcpy(outBuf, buf, *bufSize);
	return *bufSize;
}
int HandleHTTPS(char * buf, int * bufSize, char * outBuf) {
	return 0;
}
