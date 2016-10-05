import socket
port = 5999
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', port))
s.settimeout(2)
pipe = "HEAD /index.html HTTP/1.1\r\nHost: 127.0.0.1:6000\r\nConnection: Keep-Alive\r\n\r\n";
s.send(pipe)
score = 0;
i = 0
while True:
	try:
		buf = s.recv(1024)
		print buf
	except socket.timeout:
		score += 0.5
		break
	if buf == "":
		print "Server connection does not keepalive!\n"
		break;
s.close()

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', port))
s.settimeout(2)    
s.send("HEAD /index.html HTTP/1.1\r\nHost: 127.0.0.1:6000\r\nConnection: Keep-Alive\r\n\r\nHEAD /index.html HTTP/1.1\r\nHost: 127.0.0.1:6000\r\nConnection: Close\r\n\r\n")
while True:
	try:
		buf = s.recv(1024)
		print buf
	except socket.timeout:
		print "Server doesn't close connections as requested!"
		break
	if buf == "":
		score += 0.5
		break
	
print score
