all: server

server: server.c
	gcc server.c -Wall -Werror -Wextra -g -o server

clean:
	rm -f server

re: clean all
