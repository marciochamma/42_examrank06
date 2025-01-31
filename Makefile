
CC = cc -Werror -Wextra -Wall
FILE = mini_serv.c

all:
	$(CC) $(FILE) -o mini_serv

clean:
	rm -f ./mini_serv
	clear

re: clean
	make

do:
	./mini_serv 6667