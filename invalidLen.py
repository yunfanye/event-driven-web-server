import socket
port = 5999
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', port))
s.settimeout(2)
pipe = "POST /cgi/ HTTP/1.1\r\nHost: 127.0.0.1:6000\r\nConnection: Close\r\nContent-Length: -1000\r\n\r\nadsfadsfadsfadsfadsfadsfadsf";
s.send(pipe)
score = 0;
i = 0
while True:
	try:
		buf = s.recv(1024)
		print buf
	except socket.timeout:
		print "Server timeout!\n"
		break
	if buf == "":
		break;
s.close()
