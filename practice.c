#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>

// start of structs
typedef struct s_client
{
	int fd;
	int id;
	char *data;
	int datalen;
	struct s_client *next;
} t_client;

typedef	struct s_lines{
	int start, end;
	struct s_lines *next;
} t_lines;
// end of structs


// start of declrations
void	print(int fd, const char *msg);
void	error();
void	add_client(t_client **clients, int fd);
void	client_arrived(t_client *clients, int esc, fd_set *write_set, const char *msg);
void	add_str_to_client(t_client *client, const char *msg, int msglen);
void	free_lines(t_lines *lines);
int	nextnl(const char *data, int start, int end);
void	add_to_lines(t_lines **lines, int start, int end);
t_lines	*construct(t_client *client);
void	send_to_all(t_client *clients, t_client *target, fd_set *write_set, t_lines *lines);
t_client *remove_client(t_client **clients, t_client *target);
void	client_left(const char *msg, t_client *clients, fd_set *write_set);

// endof declrations

// start of globals
int last_id;
// end if globals

void	print(int fd, const char *msg)
{
	write(fd, msg, strlen(msg));
}


void	error()
{
	print(2, "Fatal error\n");
	exit(1);
}

int main(int ac, char **av)
{
	const int msg_len = 4096;
	int num_of_bytes_read;
	char msg_holder[msg_len];
	int port;
	struct sockaddr_in servaddr;
	int sockfd;
	int tmpint;
	int maxfd; // this should be updated inside the loop
	fd_set read_set, write_set, current_set;
	t_client *clients, *client_loop;

	if (ac != 2){
		print(2, "Wrong number of arguments\n");
		exit(1);
	}
	// start of setup
	clients = NULL;
	port = atoi(av[1]);
	if (port < 0)
		error();
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		error();
	
	FD_ZERO(&current_set);
	FD_SET (sockfd, &current_set);
	maxfd = sockfd + 1;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(2130706433);
	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1){
		close(sockfd);
		error();
	}
	if (listen(sockfd, 10) == -1)
		error();
	// end of setup
	


	while (1)
	{
		read_set = current_set;
		write_set = current_set;
		if (select(maxfd, &read_set, &write_set, NULL, NULL) == -1)
			continue ;

		if (FD_ISSET(sockfd, &read_set))
		{
			tmpint = accept(sockfd, NULL, NULL);

			fcntl(tmpint, F_SETFL, O_NONBLOCK); // -------- remove this line ----------
			
			maxfd = tmpint >= maxfd ? tmpint + 1 : maxfd;

			FD_SET(tmpint, &current_set);
			add_client(&clients, tmpint);
			sprintf(msg_holder, "server: client %d just arrived\n", clients->id);
			client_arrived(clients, clients->id,&write_set, msg_holder);
		} else {
			// be carful here why else block why not just remove it
			// do fctl but remove it later this is just for testing
			client_loop = clients;
			while (client_loop)
			{
				if (FD_ISSET(client_loop->fd, &read_set))
				{
					// now i need to read the entire msg
					while (1)
					{
						num_of_bytes_read = recv(client_loop->fd, msg_holder,  msg_len, 0);
						if (num_of_bytes_read < 1)
							break ;
						add_str_to_client(client_loop, msg_holder, num_of_bytes_read);
					}
					if (num_of_bytes_read == 0)
					{
						// left
						sprintf(msg_holder, "server: client %d just left\n", client_loop->id);
						close(client_loop->fd);
						FD_CLR(client_loop->fd, &current_set);
						client_loop = remove_client(&clients, client_loop);
						client_left(msg_holder, clients, &write_set);
					} else
					{
						t_lines *lines = construct(client_loop);
						send_to_all(clients, client_loop, &write_set, lines);
						free_lines(lines);
					}
				}
				if (client_loop)
					client_loop = client_loop->next;
			}
		}
	}



}

void	client_left(const char *msg, t_client *clients, fd_set *write_set)
{
	int len;

	len = strlen(msg);
	while (clients)
	{
		if (FD_ISSET(clients->fd, write_set))
			send(clients->fd, msg, len, 0);
		clients = clients->next;
	}
}

t_client *remove_client(t_client **clients, t_client *target)
{
	t_client *before_target, *ret;

	ret = target->next;
	if (*clients == target)
	{
		*clients = target->next;
	} else {
		before_target = *clients;
		while (before_target && before_target->next != target)
			before_target = before_target->next;
		before_target->next = target->next;
	}
	if (target->datalen)
		free(target->data); // possiblay never
	free(target);
	return ret;
}

void	send_to_all(t_client *clients, t_client *target, fd_set *write_set, t_lines *lines)
{
	t_lines *lineinfo;
	while (clients)
	{
		if (FD_ISSET(clients->fd, write_set) && clients->id != target->id)
		{
			lineinfo = lines;
			while (lineinfo)
			{
				send(clients->fd, target->data + lineinfo->start, lineinfo->end - lineinfo->start + 1, 0);
				lineinfo = lineinfo->next;
			}
		}
		clients = clients->next;
	}

	free(target->data);
	target->data = NULL;
	target->datalen = 0;
}

void	free_lines(t_lines *lines)
{
	t_lines *nx;
	while (lines)
	{
		nx = lines->next;
		free(lines);
		lines = nx;
	}
}

int	nextnl(const char *data, int start, int end)
{
	while (start < end && data[start++] != '\n');
	return start - 1;
}


void	add_to_lines(t_lines **lines, int start, int end)
{
	t_lines *ln;
	t_lines *tmp;
	ln = malloc(sizeof(t_lines));
	
	if (!ln)
		error();
	ln->start = start;
	ln->end = end;
	ln->next = NULL;
	if (*lines)
	{
		tmp = *lines;
		while (tmp && tmp->next)
			tmp = tmp->next;
		tmp->next = ln;
	}
	else
		*lines = ln;
}

t_lines	*construct(t_client *client)
{
	t_lines *lines;

	int start;
	int nlpos;

	start = 0;
	lines = NULL;

	while (start < client->datalen) {
		nlpos = nextnl(client->data, start, client->datalen);
		add_to_lines(&lines, start, nlpos);
		start = nlpos + 1;
	}
	return lines;
}

void	add_str_to_client(t_client *client, const char *msg, int msglen)
{
	int i, j;
	char *newbuf = malloc(sizeof(char) * (client->datalen + msglen)); // allocated

	
	i = 0;
	j = 0;
	if (!newbuf)
		error();
	while (i < client->datalen)
	{
		newbuf[i] = client->data[i];
		++i;
	}
	while (i < client->datalen + msglen)
	{
		newbuf[i] = msg[j];
		++i;
		++j;
	}

	if (client->datalen)
		free(client->data);
	client->data = newbuf;
	client->datalen += msglen;
}


void	client_arrived(t_client *clients, int esc, fd_set *write_set, const char *msg)
{
	while (clients)
	{
		if (FD_ISSET(clients->fd, write_set) && clients->id != esc)
			send(clients->fd, msg, strlen(msg), 0);
		clients = clients->next;
	}
}


void	add_client(t_client **clients, int fd)
{
	t_client *cl;

	cl = malloc(sizeof(t_client)); // allocated
	if (!cl)
		error();

	cl->fd = fd;
	cl->id = last_id++;
	cl->data = NULL;
	cl->datalen = 0;

	if (*clients)
	{
		cl->next = *clients;
		*clients = cl;
	}
	else
	{
		cl->next = NULL;
		*clients = cl;	
	}
}

/* NOTS
 * do not forget to close fd when client left
 * do not forget to update maxfd when client is connected
 * do not forget to add new fd returned by accept to current set
 * 
 *
 * --> if it didnt work try add client ro end of lined list
 */
