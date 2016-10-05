import subprocess
import signal
import os
import time

args = ['./lisod', '5788', '4200', '../tmp/lisod0.log', '../tmp/lisod.lock', '../tmp/www', '../tmp/cgi/cgi_script.py', '../tmp/grader.key', '../tmp/grader.crt']
for x in range(0, 100):
	subprocess.call(args)
	f = open('../tmp/lisod.lock', 'r')
	pid = f.readline().strip()
	try:
		pid = int(pid)
	except ValueError:
		raise Exception("Lockfile does not have a valid pid!")
	if pid <= 0:
		raise Exception("Lockfile does have an invalid pid!")

	try:
		os.kill(pid, 9)
	except OSError:
		Exception("But pid %d is dead or never lived before!" % pid)
	time.sleep(1)
	try:
		os.kill(pid, 0)
	except OSError:
		print "%d th: Success killed the process" % x
	else:
		print "Not killed"
		print "Server running on pid %d" % pid
		break
