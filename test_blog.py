
import socket


port = 7070
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', port))
s.settimeout(2)
pipe = "GET /cgi/ HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:41.0) Gecko/20100101 Firefox/41.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nReferer: http://127.0.0.1:5000/login\r\nCookie: session=.eJyrVopPy0kszkgtVrKKrlZSKIFQSUpWSknhYVXJRm55UYG2tkq1OlDR8HBLQ0-PlJzk3ND0JHfLvCijsGxP95xSpdrY2lgdpZz89PTUlPjMPCWrkqLS1FoAp6se9g.CPmykg.u5uFm-J_eqTwfA-wAgMOMHbWSUU\r\nConnection: keep-alive\r\n\r\n" % port
print pipe
s.send(pipe)
while True:
	try:
		buf = s.recv(1024)
		print buf
	except socket.timeout:
		break
	if buf == "":
		break
s.close()





port = 5000
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', port))
s.settimeout(2)
pipe = "GET / HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:41.0) Gecko/20100101 Firefox/41.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nReferer: http://127.0.0.1:5000/login\r\nCookie: session=.eJyrVopPy0kszkgtVrKKrlZSKIFQSUpWSknhYVXJRm55UYG2tkq1OlDR8HBLQ0-PlJzk3ND0JHfLvCijsGxP95xSpdrY2lgdpZz89PTUlPjMPCWrkqLS1FoAp6se9g.CPmk_w.t23eizellwvykzCDPRkg9r8O2gw\r\nConnection: keep-alive\r\n\r\n" % port
print pipe
s.send(pipe)
while True:
	try:
		buf = s.recv(1024)
		print buf
	except socket.timeout:
		break
	if buf == "":
		break
s.close()

