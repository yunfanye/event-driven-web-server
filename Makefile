################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for lisod server              #
#                                                                              #
# Authors: Yunfan Ye <yunfany@andrew.cmu.edu>		                           #
#                                                                              #
################################################################################

HTTP=5890
HTTPS=4890

default: clean lisod

lisod: common.o requestHandler.o CGIHandler.o lisod.o y.tab.o lex.yy.o 
	@gcc $^ -o lisod -Wall -Werror -L/usr/local/ssl/lib -lssl -lcrypto

echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror
	
lex.yy.c: lexer.l
	flex $^

common.o: common.c
	gcc -g -c $^ -o $@

requestHandler.o: requestHandler.c
	gcc -g -c $^ -o $@
	
CGIHandler.o: CGIHandler.c
	gcc -g -c $^ -o $@

lisod.o: lisod.c
	gcc -g -c $^ -o $@

y.tab.o: y.tab.c
	gcc -g -c $^ -o $@

lex.yy.o: lex.yy.c
	gcc -g -c $^ -o $@

y.tab.c: parser.y
	yacc -d $^
	
run: clean lisod
	(./lisod $(HTTP) $(HTTPS) ../tmp/lisod0.log ../tmp/lisod.lock ../tmp/www ../tmp/cgi/cgi_script.py ../tmp/grader.key ../tmp/grader.crt)

run2: clean lisod
	(./lisod $(HTTP) $(HTTPS) ../tmp/lisod0.log ../tmp/lisod.lock ../tmp/www ./flaskr/flaskr.py ../tmp/grader.key ../tmp/grader.crt)

test: ab_test siege_test

ab_test:
	(ab -n 100 -c 100 http://127.0.0.1:$(HTTP)/)
	
siege_test:
	(siege -r 300 -c 300 -b http://127.0.0.1:$(HTTP)/)

clean:
	rm -f *~ lisod *.o *.tar *.zip *.gzip *.bzip *.gz y.tab.c y.tab.h lex.yy.c

handin_%:
	(make clean; git commit -a -m "handin"; git tag -d checkpoint-$*; git tag -a checkpoint-$* -m "handin"; cd ..; tar cvf yunfany.tar 15-441-project-1 --exclude *.py --exclude test --exclude tmp --exclude parser_cp2 --exclude helper.txt)
	
handin:
	(make clean; cd ..; tar cvf yunfany.tar 15-441-project-1 --exclude *.py --exclude test --exclude tmp --exclude parser_cp2 --exclude helper.txt)
