#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>


struct s_client{
	int fd;
	int id;
	char *data;
	int datalenght;
	struct s_client *next;
};

struct s_lines{
	int start;
	int end;
	struct s_lines *next;
};

typedef struct s_lines t_lines;

typedef struct s_client t_client;


// globals start
unsigned	client_id;
// globals end

void	sendtoall(t_client *clients, t_client *whosent, t_lines *lines,  fd_set *read_set);
void	free_client_buffer(t_client *client);
void	free_lines(t_lines *lines);

void error()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}
char	*strjoin(char *s1, int l1, char *s2, int l2)
{
	int i = 0;
	int j = 0;
	char *newbuff = malloc(l1 + l2);

	if (!newbuff)
		error();
	
	while (i < l1){
		newbuff[i] = s1[i];
		++i;
	}
	while (j < l2)
	{
		newbuff[i] = s2[j];
		++i;
		++j;
	}
	free(s1);
	return newbuff;
}

int indexof(const char *data, char c, int start, int end)
{
	while (start < end && data[start++] != c);
	return start - 1;
}

void	add_newline(t_lines **head, t_lines **end, int start, int endpos)
{
	t_lines *tmp  = malloc(sizeof(t_lines));
	if (!tmp)
		error();


	tmp->start = start;
	tmp->end = endpos;
	tmp->next = NULL;
	if (*head)
	{
		(*end)->next = tmp;
	} else {
		(*head) = tmp;
	}
	*end = tmp;
}


// how many new lines could be in line ?
t_lines	*construct(const char *data, int datalen)
{
	t_lines		*lines, *end;
	int			start, nlpos;

	start = 0;
	lines = NULL;

	while (start < datalen)
	{
		nlpos = indexof(data, '\n', start, datalen);
		add_newline(&lines, &end, start, nlpos);
		start = nlpos + 1;
	}
	return lines;
}



void	print(int fd, const char *msg)
{
	write(fd, msg, strlen(msg));
}

void add_client(int fd, t_client **clients)
{
	t_client *new = malloc(sizeof(t_client));

	if (!new)
		error();

	new->fd = fd;
	new->id = client_id;
	new->data = NULL;
	new->datalenght = 0;
	if (*clients)
	{
		new->next = *clients;
		*clients = new;
	} else
	{
		new->next = NULL;
		*clients = new;
	}
	++client_id;
}

void clientarrived(const char *msg, t_client *clients, fd_set *write_set, int id)
{
	while (clients)
	{
		if (FD_ISSET(clients->fd, write_set) &&  clients->id != id){
			send(clients->fd, msg, strlen(msg), 0);
		}
		clients = clients->next;
	}
}


void print_clients(t_client *clients)
{
	char msg[128];
	while (clients)
	{
		sprintf(msg, "client fd: %d id: %d\n", clients->fd, clients->id);
		print(1, msg);
		clients = clients->next;
	}
}

t_client	*remove_client(t_client **clients,  t_client *target)
{
	t_client *nx;

	nx = *clients;
	if (nx == target) {
		*clients = nx->next;
	} else {
		while (nx && nx->next != target)
			nx = nx->next;
		nx->next = target->next;
	}
	nx = target->next;
	free(target);
	return nx;
}
void	client_left(t_client *clients, int id, fd_set *write_set)
{
	char msg[128];
	int len;


	sprintf(msg, "server: client %d just left\n", id);
	len = strlen(msg);
	while (clients)
	{
		if (FD_ISSET(clients->fd, write_set))
			send(clients->fd, msg, len, 0);
		clients = clients->next;
	}
}

int main(int argc, char **argv)
{
	const int max_buffer_size = 4096;
	short success_before;
	int num_of_bytes_read;
	int port;
	char msg[128];
	char buffer[max_buffer_size];
	int tmpint;
	struct sockaddr_in servaddr;
	int sockfd;
	int maxfd;
	t_client *clients, *client_tmp;

	fd_set current_set, read_set, write_set;

	if (argc != 2)
	{
		print(2, "Wrong number of arguments\n");
		exit(1);
	}

	port = atoi(argv[1]);
	if (port < 0)
		error();
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		error();
	// intial start
	
	maxfd = sockfd + 1;
	clients = NULL;
	bzero(&servaddr, sizeof(servaddr));
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(port);
	
	FD_ZERO(&current_set);
	FD_SET(sockfd, &current_set);

	// intial end
	
	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
		error();
	if (listen(sockfd, 10) == -1)
		error();


	while (1)
	{
		read_set = current_set;
		write_set = current_set;

		if (select(maxfd, &read_set, &write_set, NULL, NULL) == -1)
			error();

		if (FD_ISSET(sockfd, &read_set))
		{
			// means new client is just connected
			// add the new client and notify others
			tmpint = accept(sockfd, NULL, NULL);
			fcntl(tmpint, F_SETFL, O_NONBLOCK);
			maxfd = tmpint >= maxfd ? tmpint + 1 : maxfd;

			add_client(tmpint, &clients);
			FD_SET(tmpint, &current_set);
			// send message to others
			sprintf(msg, "server: client %d just arrived\n", clients->id); 
			// will work only if new client is add first to clients list

			clientarrived(msg, clients, &write_set, clients->id);
		} else {
			// here i will check others and see if someone left or writing
			client_tmp = clients;
			while (client_tmp)
			{
				if (FD_ISSET(client_tmp->fd, &read_set))
				{
					success_before = -3;
					do
					{
						num_of_bytes_read = recv(client_tmp->fd, buffer, max_buffer_size, 0);
						if (success_before == -3)
							success_before = num_of_bytes_read == -1 ? -1 : 1;
						// joing new data to the client
						if (num_of_bytes_read > 0) {
							client_tmp->data = strjoin(client_tmp->data, client_tmp->datalenght, buffer, num_of_bytes_read);
							client_tmp->datalenght += num_of_bytes_read;;
						}
					}while (num_of_bytes_read > 0);
					
					if (success_before == -1)
					{
						// means error happend should be responded
						printf("error\n");
					} else if (num_of_bytes_read == 0)
					{
						// means client has just left
						printf("client left\n");
						FD_CLR(client_tmp->fd, &current_set);
						close(client_tmp->fd);
						tmpint = client_tmp->id;
						client_tmp = remove_client(&clients, client_tmp);
						// send message to others 
						client_left(clients, tmpint, &write_set);
					} else {
						// parse string notify everyone
						printf("client is writing\n");
						t_lines *lines = construct(client_tmp->data, client_tmp->datalenght);
						sendtoall(client_tmp, clients, lines, &write_set);
						free_lines(lines);
						free_client_buffer(client_tmp);
					}
				}
				if (client_tmp)
					client_tmp = client_tmp->next;
			}
		}
	}
}

void	free_lines(t_lines *lines)
{
	t_lines *n;

	if (lines)
	{
		n = lines->next;
		while (lines)
		{
			free(lines);
			lines = n;
			if (lines)
				n = lines->next;
		}
	}
}

void	free_client_buffer(t_client *client)
{
	free(client->data);
	client->datalenght = 0;
	client->data = NULL;
}


void	sendtoall(t_client *clients, t_client *whosent, t_lines *lines, fd_set *read_set)
{
	char start_msg[128];
	int len;
	t_lines *tmplines;


	sprintf(start_msg, "client %d: ", whosent->id);
	len = strlen(start_msg);
	while (clients)
	{
		if (FD_ISSET(clients->fd, read_set) && clients->id != whosent->id)
		{
			tmplines = lines;
			while (tmplines)
			{
				send(clients->fd, start_msg, len, 0);
				send(clients->fd, whosent->data + tmplines->start, tmplines->end - tmplines->start + 1, 0);
				tmplines = tmplines->next;
			}
		}
		clients = clients->next;
	}
}
