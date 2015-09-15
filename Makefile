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

lisod:
	@gcc lisod.c requestHandler.c -o lisod -Wall -Werror

echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror

clean:
	rm -f *~ lisod *.o *.tar *.zip *.gzip *.bzip *.gz
	
handin:
	(make clean; cd ..; tar cvf yunfany.tar 15-441-project-1 --exclude *.py)
