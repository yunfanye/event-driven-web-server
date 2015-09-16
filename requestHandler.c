#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "requestHandler.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


const char * _server_header = "Server: Liso/1.0\r\n";
const char * _connect_header = "Connection: close\r\n";

struct type_map {
	char surfix[TINY_BUF_SIZE];
	char type[TINY_BUF_SIZE];
} _type_map_entries[] = {
    {".html", "text/html"},
    {".css" , "text/css"},
    {".png" , "image/png"},
    {".jpg" , "image/jpeg"},
    {".gif" , "image/gif"}
};

char is_bad_request;
char had_request_line;
char _method[SMALL_BUF_SIZE];
char _uri[SMALL_BUF_SIZE];
char _prot[SMALL_BUF_SIZE];
char _token[SMALL_BUF_SIZE];
char _text[SMALL_BUF_SIZE];

extern char _www_root[SMALL_BUF_SIZE];

enum parse_state {
	STATE_START = 0,
	STATE_CR,
	STATE_CRLF,
	STATE_CRLFCR,
	STATE_CRLFCRLF
};

extern void set_parsing_buf(char *buf, size_t siz);

#define CONVERT_TO_LOWER(str, i)										\
{																		\
	i = 0;																\
	while(str[i]) { 													\
		str[i] = tolower(str[i]);										\
		i++;															\
	}																	\
}																		\

#define MAP_SURFIX_TO_TYPE(msurfix, mtype, i)							\
{																		\
	char * type; 														\
	for(i = 0; i < sizeof(_type_map_entries)/sizeof(struct type_map); 	\
		i++) {															\
		if(!strcmp(_type_map_entries[i].surfix, msurfix)) {				\
			type = _type_map_entries[i].type;							\
			memcpy(mtype, type, strlen(type) + 1);						\
		}																\
	}																	\
}

int HandleHTTP(char * buf, int buf_size, char * out_buf) {
	int i;
	int index = 0, last_index = 0;
	char work_buf[SMALL_BUF_SIZE];
	char * char_tmp;
	enum parse_state state = STATE_START;
	int response_size;
	
	printf("---------------parser begin-----------------\r\n");
	
	is_bad_request = 0;
	had_request_line = 0;
	while(index < buf_size && state != STATE_CRLFCRLF) {
		char expected = 0;
		switch(state) {
			case STATE_START:
				expected = '\r';
				break;
			case STATE_CRLF:
				/* parse one line */
				if(had_request_line) {
					set_parsing_buf(buf + last_index, index - last_index - 2);	
					yyparse();
					last_index = index;
				} else {
					memcpy(work_buf, buf, index - 2);
					work_buf[index + 1] = '\0';
					if(sscanf(work_buf, "%s %s %s", _method, _uri, _prot) != 3)
						is_bad_request = 1;
					had_request_line = 1;
					last_index = index;
				}
				expected = '\r';
				break;
			case STATE_CR:
			case STATE_CRLFCR:
				expected = '\n';
				break;
			default:
				state = STATE_START;
				continue;
		}

		if(buf[index] == expected)
			state++;
		else
			state = STATE_START;
		index++;
	}

	if(state == STATE_CRLFCRLF) {
		/* FOUND CRLF-CRLF */
		/* convert to lower case */
		CONVERT_TO_LOWER(_method, i);
		response_size = get_response(out_buf);
	} else {
		/* Could not find CRLF-CRLF*/
		printf("Failed: Could not find CRLF-CRLF\n");
	}
		
	printf("%s: %d", out_buf, response_size);
	return response_size;
}

int get_response(char * out_buf) {
	/* find uri status */
	int i;
	int fd;
	int response_len, append_len;
	off_t total_size;
	enum status code = S_200_OK;
	char status_message[TINY_BUF_SIZE];	
	time_t timer;
    char current_time[TINY_BUF_SIZE], last_modified_time[TINY_BUF_SIZE];
    char content_type[TINY_BUF_SIZE];
    char surfix_buf[TINY_BUF_SIZE];
    char www_path[SMALL_BUF_SIZE];
    char * char_tmp;
    char response_body[BUF_SIZE];
    off_t read_size, body_size;
    struct stat statbuf;            /* file statistics */
    
	/* get current time */
    time(&timer);
	asctime_r(localtime(&timer), current_time);	
	current_time[strlen(current_time) - 1] = '\0'; /* remove \n */
	/* generate the whole path */
	memset(www_path, 0, SMALL_BUF_SIZE);
	strcat(www_path, _www_root);
	strcat(www_path, _uri);
	/* get file info */
	if(is_bad_request)
		code = S_400_BAD_REQUEST;
	if(stat(www_path, &statbuf) < 0) 
		code = S_404_NOT_FOUND;
	else {
		if(S_ISDIR(statbuf.st_mode))
			strcat(www_path, "/index.html");
		total_size = statbuf.st_size;
		/* get last modified time */
		asctime_r(localtime(&statbuf.st_mtime), last_modified_time);	
		last_modified_time[strlen(last_modified_time) - 1] = '\0';
		/* get content type */
		char_tmp = strrchr(www_path, '.');
		memcpy(surfix_buf, char_tmp, strlen(char_tmp) + 1);
		CONVERT_TO_LOWER(surfix_buf, i);
		MAP_SURFIX_TO_TYPE(surfix_buf, content_type, i);
		if(!strcmp(_method, "get")) {
			if((fd = open(www_path, O_RDONLY)) < 0)
				code = S_500_INTERNAL_SERVER_ERROR;
			else {
				read_size = MIN(total_size, BUF_SIZE);
				if((body_size = get_body(fd, response_body, read_size)) != read_size)
					code = S_503_SERVICE_UNAVAILABLE;
			}
		}
		else if(!strcmp(_method, "head"))
			body_size = 0;
		else if(!strcmp(_method, "post"))
			body_size = 0; 
		else
			code = S_501_NOT_IMPLEMENTED; 
	}
	
	get_message(code, status_message);
	if(code == S_200_OK) {
		sprintf(out_buf, "HTTP/1.1 %s\r\nDate: %s\r\nLast-Modified: %s\r\n"
			"Content-Length: %lu\r\nContent-Type: %s\r\n%s%s\r\n", 
			status_message, current_time, last_modified_time, total_size,
			content_type, _server_header, _connect_header);
		/* append body to the header */
		response_len = strlen(out_buf);
		append_len = MIN(body_size, BUF_SIZE - response_len);
		if(append_len > 0)
			memcpy(out_buf + response_len, response_body, append_len);
		return response_len + (append_len == 0 ? 1 : append_len);
	} else {
		sprintf(out_buf, "HTTP/1.1 %s\r\nDate: %s\r\n%s%s\r\n", 
			status_message, current_time, _server_header, _connect_header);
		response_len = strlen(out_buf) + 1;
		return response_len;
	}
}

off_t get_body(int fd, char * out_buf, off_t size) {
    off_t readleft = size;
    off_t readret;

    while (readleft > 0) {
		if ((readret = read(fd, out_buf, readleft)) < 0) {
			if (errno != EINTR)
				return -1;     
		} 
		else if (readret == 0)
			break;
		else {             
			readleft -= readret;
			out_buf += readret;
		}
    }
    return (size - readleft);   
}

int get_message(enum status code, char * msg) {
	char * msg_tmp;
	int msg_len;
	switch(code) {
		case S_200_OK:
			msg_tmp = "200 OK";
			break;
		case S_400_BAD_REQUEST:
			msg_tmp = "400 BAD REQUEST";
			break;
    	case S_404_NOT_FOUND:
			msg_tmp = "404 NOT FOUND";
			break;
    	case S_411_LENGTH_REQUIRED:
			msg_tmp = "411 LENGTH REQUIRED";
			break;
    	case S_500_INTERNAL_SERVER_ERROR:
			msg_tmp = "500 INTERNAL SERVER ERROR";
			break;
    	case S_501_NOT_IMPLEMENTED:
			msg_tmp = "501 NOT IMPLEMENTED";
			break;
    	case S_503_SERVICE_UNAVAILABLE:
			msg_tmp = "503 SERVICE UNVAILABLE";
			break;
    	case S_505_HTTP_VERSION_NOT_SUPPORTED:
			msg_tmp = "505 HTTP VERSION NOT SUPPORTED";
			break;
	}
	msg_len = strlen(msg_tmp);
	memcpy(msg, msg_tmp, msg_len + 1);
	return 1;
}

int HandleHTTPS(char * buf, int bufSize, char * out_buf) {
	return 0;
}
