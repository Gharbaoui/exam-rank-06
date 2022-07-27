#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

// start of macros
#define RECV_SIZE 4096
// end od macros

// start of headers
void	error();
void	print(int fd, const char *msg);
// end of headers

// start of structs
struct s_client{
	unsigned int fd;
	int id;
	struct s_client *next; 
};

typedef struct s_client t_client;
// end of structs

// start of glabals
t_client *g_last_client;
// end of globals


void add_client(int fd, int id, t_client **clients)
{
	t_client *new = malloc(sizeof(t_client));
	if (!new)
		error();
	new->fd = fd;
	new->id = id;
	new->next = NULL;
	if (*clients){
		g_last_client->next = new;
	}
	else
	{
		*clients = new;
	}
	g_last_client = new;
}

t_client	*remove_client(t_client *to_remove, t_client **clients)
{
	// to do later
	t_client *tmp = *clients;
	close(to_remove->fd);
	if (tmp == to_remove)
	{
		*clients = to_remove->next;
		free(to_remove);
		return NULL;
	}
	while (tmp && tmp->next != to_remove)
		tmp = tmp->next;

	tmp->next = to_remove->next;
	free(to_remove);
	return tmp->next;
}


void	sendtoallexecpt(int escape, const char *msg, int len, t_client *clients, fd_set *write_set)
{
	while (clients)
	{
		if (clients->id != escape && FD_ISSET(clients->fd, write_set))
			send(clients->fd, msg, len, 0);
		clients = clients->next;
	}
}



void	sendall(int id, const char *msg, int msg_size, t_client *clients, fd_set *write_set) {
	int i = 0;
	int last_pos;
	last_pos = 0;
	char msg_start[128];
	short len;
	t_client *tmp;



	sprintf(msg_start, "client %d: ", id);
	len = strlen(msg_start);

	while (i < msg_size)
	{
		if (msg[i] == '\n')
		{
			tmp = clients;
			while (tmp)
			{
				if (tmp->id != id && FD_ISSET(tmp->fd, write_set))
				{
					send(tmp->fd, msg_start, len, 0);
					send (tmp->fd, msg + last_pos, i - last_pos + 1, 0);
					print(2, "sent\n");
					write(2, msg + last_pos, i - last_pos + 1);
				}
				tmp = tmp->next;
			}
			last_pos = i + 1;
		}
		++i;
	}
}

void	print(int fd, const char *msg)
{
	write(fd, msg, strlen(msg));
}

void	error()
{
	print(2, "Fatal error\n");
	exit(1);
}

int main(int argc, char **argv) {
	char	msg[128];
	char rcv_msg[RECV_SIZE];
	t_client *clients, *client_loop_read;
	int sockfd, connfd, len, port;
	unsigned int maxfd;
	int read_size;
	unsigned int ready_descriptors;
	int	lastid;
	fd_set current_set, read_set, write_set;
	struct sockaddr_in servaddr, cli; 

	if (argc != 2)
	{
		print(2, "Wrong number of arguments\n");
		exit(1);
	}

	port = atoi(argv[1]);
	if (port < 0)
		error();

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		error();
	bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		error();
	if (listen(sockfd, 10) != 0)
		error();

	// start of setup
	FD_ZERO(&current_set);
	FD_SET(sockfd, &current_set);
	maxfd = sockfd + 1;
	lastid = 0;
	clients = NULL;
	g_last_client = NULL;
	// end of setup
	

	while (1)
	{
		read_set = current_set;
		write_set = current_set;
		ready_descriptors = select(maxfd, &read_set, &write_set, NULL, NULL);
		if (ready_descriptors <= 0)
			continue ;
		if (FD_ISSET(sockfd, &read_set))
		{
			connfd = accept(sockfd, NULL, NULL); // maybe changing this would help to detect if cliet has left
			add_client(connfd, lastid, &clients);
			sprintf(msg, "server: client %d just arrived\n", lastid);
			sendtoallexecpt(lastid, msg, strlen(msg), clients, &write_set);
			++lastid;
			++maxfd;
			FD_SET(connfd, &current_set);
		} else {
			client_loop_read = clients;
			while (client_loop_read)
			{
				if (FD_ISSET(client_loop_read->fd, &read_set))
				{
					read_size = recv(client_loop_read->fd, rcv_msg, RECV_SIZE, 0);
					if (read_size <= 0) {
						// probably close connection
						close(client_loop_read->fd);
						sprintf(msg, "server: client %d just left\n", client_loop_read->id);
						client_loop_read = remove_client(client_loop_read, &clients);
						sendtoallexecpt(-1, msg, strlen(msg), clients, &write_set);
						if (!client_loop_read)
							break ;
					} else
						sendall(client_loop_read->id, rcv_msg, read_size, clients, &write_set);
				}
				client_loop_read = client_loop_read->next;
			}
		}
	}
}
