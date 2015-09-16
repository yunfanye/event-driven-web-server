################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: lisod

lisod: common.o lisod.o requestHandler.o y.tab.o lex.yy.o 
	@gcc $^ -o lisod -Wall -Werror

echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror
	
lex.yy.c: lexer.l
	flex $^

common.o: common.c
	gcc -g -c $^ -o $@

lisod.o: lisod.c
	gcc -g -c $^ -o $@

requestHandler.o: requestHandler.c
	gcc -g -c $^ -o $@

y.tab.o: y.tab.c
	gcc -g -c $^ -o $@

lex.yy.o: lex.yy.c
	gcc -g -c $^ -o $@

y.tab.c: parser.y
	yacc -d $^

clean:
	rm -f *~ lisod *.o *.tar *.zip *.gzip *.bzip *.gz y.tab.c y.tab.h lex.yy.c
	
handin:
	(make clean; cd ..; tar cvf yunfany.tar 15-441-project-1 --exclude *.py --exclude test --exclude tmp --exclude parser_cp2)
