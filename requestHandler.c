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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

/* const strings */
const char * _content_len_token = "Content-Length";
const char * _server_header = "Server: Liso/1.0\r\n";
const char * _connect_header = "Connection: close\r\n";
const char * error_msg_tpl = "<head><title>Error response</title></head><body><h1>Error response</h1><p>Error: %s</p></body>";

/* surfix content-type mapping table */
struct type_map _type_map_entries[] = {
    {".html", "text/html"},
    {".css" , "text/css"},
    {".png" , "image/png"},
    {".jpg" , "image/jpeg"},
    {".gif" , "image/gif"}
};

/* Convert every character of a string to lower case */
#define CONVERT_TO_LOWER(str, i)										\
{																		\
	i = 0;																\
	while(str[i]) { 													\
		str[i] = tolower(str[i]);										\
		i++;															\
	}																	\
}																		\

/* Map the surfix of a file to Content-Type header */
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

/* HandleHTTP - handle http request 
 * buf - request content
 * buf_size - request size
 * out_buf - response content
 * socket - corresponding socket
 * return value - response size
 */
int HandleHTTP(char * buf, int * ori_buf_size, char * out_buf, int socket) {
	int i;
	int index = 0, last_index = 0;
	char work_buf[SMALL_BUF_SIZE];
	char * char_tmp;
	enum parse_state state = STATE_START;
	int response_size;
	char is_set_browser; /* in case no browser info */
	char browser_info[SMALL_BUF_SIZE];
	/* we only care about ipv4, if ipv6 this should be sockaddr_storage */
	struct sockaddr_in peer_addr;
	socklen_t peer_len;
	int peer_port;
	char ip_addr[TINY_BUF_SIZE];
	char addition_info[TINY_BUF_SIZE];
	char has_content_len; /* POST request should contain Content-Length */
	int buf_size = * ori_buf_size;

	has_content_len = 0;
	is_set_browser = 0;
	had_request_line = 0;
	code = S_200_OK;
	
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
					/* yyparse return 0 on success */
					if(yyparse()) {
						code = S_400_BAD_REQUEST;
					}
					else {
						/* A post request should have content length */
						if(!strcmp(_method, "post") && !strcmp(_token, _content_len_token)) {
							/* parse the text using atoi when post is supported */
							has_content_len = 1;
						}
						if(!strcmp(_token, "User-Agent")) {						
							is_set_browser = 1;
							memcpy(browser_info, _text, strlen(_text) + 1);
						}
					}
					last_index = index;
				} else {
					memcpy(work_buf, buf, index - 2);
					work_buf[index - 2] = '\0';
					/* we only allow _prot == 'HTTP/1.1', _method in 'get,
					 * post, head', and _uri a valid file or dir */
					if(sscanf(work_buf, "%s %s %s", _method, _uri, _prot) != 3)
						code = S_400_BAD_REQUEST;
					else {
						/* convert method to lower case */
						CONVERT_TO_LOWER(_method, i);
					}
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
		/* FOUND CRLF-CRLF, header ends */
		/* if it is a post request without content length header, return 411 */
		if(code != S_400_BAD_REQUEST && 
			!has_content_len && !strcmp(_method, "post"))
			code = S_411_LENGTH_REQUIRED;
		/* get response */
		response_size = get_response(out_buf);
				
		/* for valid http packet, log the ip address, webbrowser & code */
		if(is_set_browser)
			msg_log("User-Agent", browser_info);
		/* get and log the ip address & port */
		peer_len = sizeof(struct sockaddr_in);
		getpeername(socket, (struct sockaddr *) &peer_addr, &peer_len);
		/* we focus on only ipv4, i.e. sin_familiy is AF_INET */
		peer_port = ntohs(peer_addr.sin_port);
		sprintf(addition_info, ":%d -- %s", peer_port, status_message);
		inet_ntop(AF_INET, &peer_addr.sin_addr, ip_addr, TINY_BUF_SIZE);
		strcat(ip_addr, addition_info);
		msg_log("IP", ip_addr);
	} else {
		/* Could not find CRLF-CRLF*/
		response_size = 0;
		error_log("Failed: Could not find CRLF-CRLF\n");
	}
	return response_size;
}

int get_response(char * out_buf) {
	/* find uri status */
	int i;
	int fd;
	int response_len, append_len;
	off_t total_size;
	time_t timer;
    char current_time[TINY_BUF_SIZE], last_modified_time[TINY_BUF_SIZE];
    char content_type[TINY_BUF_SIZE];
    char surfix_buf[TINY_BUF_SIZE];    
    char * char_tmp;
    char response_body[BUF_SIZE];
    char error_msg[SMALL_BUF_SIZE];
    off_t read_size, body_size;
    struct stat statbuf;            /* file statistics */
    
	/* get current time in GMT */
    time(&timer);
    strftime(current_time, TINY_BUF_SIZE, "%a, %d %h %Y %H:%M:%S %Z", 
    	gmtime(&timer));
	current_time[strlen(current_time)] = '\0'; /* remove \n */
	/* generate the whole path */
	memset(_www_path, 0, SMALL_BUF_SIZE);
	strcat(_www_path, _www_root);
	strcat(_www_path, _uri);
	/* init */
	_has_remain_bytes = 0;
	/* get file info */

	if(code != S_200_OK)
		;/* do something to handle pervious error */
	else if(strcmp(_prot, "HTTP/1.1"))
		code = S_505_HTTP_VERSION_NOT_SUPPORTED;
	else if(stat(_www_path, &statbuf) < 0) 
		code = S_404_NOT_FOUND;
	else {
		if(S_ISDIR(statbuf.st_mode)) {
			strcat(_www_path, "index.html");
			if(stat(_www_path, &statbuf) < 0) 
				code = S_404_NOT_FOUND;
		}
		/* Check whether the file has read permission for vistors */
		if(!(statbuf.st_mode & S_IROTH))
			code = S_403_FORBIDDEN;
		/* get total file size */
		total_size = statbuf.st_size;
		/* get last modified time in GMT */
		strftime(last_modified_time, TINY_BUF_SIZE, "%a, %d %h %Y %H:%M:%S %Z",
    		gmtime(&statbuf.st_mtime));	
		last_modified_time[strlen(last_modified_time)] = '\0';
		/* get content type */
		char_tmp = strrchr(_www_path, '.');
		memcpy(surfix_buf, char_tmp, strlen(char_tmp) + 1);
		CONVERT_TO_LOWER(surfix_buf, i);
		MAP_SURFIX_TO_TYPE(surfix_buf, content_type, i);
		if(!strcmp(_method, "get")) {
			if((fd = open_file(_www_path, O_RDONLY)) < 0)
				code = S_500_INTERNAL_SERVER_ERROR;
			else {
				read_size = MIN(total_size, BUF_SIZE);
				if((body_size = get_body(fd, response_body, read_size)) != read_size)
					code = S_503_SERVICE_UNAVAILABLE;
				close_file(fd);
			}
		}
		else if(!strcmp(_method, "head"))
			body_size = 0;
		else if(!strcmp(_method, "post"))
			body_size = 0; /* ignore post request right now */
		else
			code = S_501_NOT_IMPLEMENTED;
	}
	
	get_message(code, status_message);
	if(code == S_200_OK) {
		sprintf(out_buf, "HTTP/1.1 %s\r\n" "%s" "Date: %s\r\n" "%s"
			"Content-Type: %s\r\n" "Content-Length: %lu\r\n" 
			"Last-Modified: %s \r\n\r\n", status_message, _server_header, 
			current_time, _connect_header, content_type, total_size,
			last_modified_time);
		/* append body to the header */
		response_len = strlen(out_buf);
		append_len = MIN(body_size, BUF_SIZE - response_len);		
		if(append_len > 0) {
			memcpy(out_buf + response_len, response_body, append_len);
		}
		/* for big files, write when buf is full; hence record the remain */		
		if(!strcmp(_method, "get") && total_size > append_len) {
			_has_remain_bytes = 1;
			_remain_bytes = total_size - append_len;
			_file_offset = append_len;
		}
		return response_len + (append_len == 0 ? 1 : append_len);
	} else {
		/* In reality, when users encounters an error, say 404, they should
		 * be redirected to the corresponding page, say 404.html; yet in this
		 * tiny server, we just print the message */
		
		if(!strcmp(_method, "head")) {
			sprintf(out_buf, "HTTP/1.1 %s\r\n" "Date: %s\r\n"
				"%s" "Content-Type: text/html\r\n" "%s\r\n",
				status_message, current_time, _server_header, _connect_header);
		}
		else {
			sprintf(error_msg, error_msg_tpl, status_message);
			response_len = strlen(error_msg);
			sprintf(out_buf, "HTTP/1.1 %s\r\n" "Date: %s\r\n" 
				"Content-Length: %d\r\n" "%s"
				"Content-Type: text/html\r\n" "%s\r\n%s", 
				status_message, current_time, response_len, 
				_server_header, _connect_header, error_msg);
		}
		response_len = strlen(out_buf) + 1;
		return response_len;
	}
}

/* read until size bytes */
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
		case S_403_FORBIDDEN:
			msg_tmp = "403 FORBIDDEN";
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
	memcpy(out_buf, buf, bufSize);
	return bufSize;
}
