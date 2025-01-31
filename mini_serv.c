

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/ip.h>

int count = 0;
int max_fd = 0;
int ids[10000];

char *msgs[10000];
char buffer_read[1024 * 2];
char buffer_write[1024 * 2];

fd_set read_set, write_set, current_set;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void error_msg(int index)
{
	if (index == 1)
		write(2, "Wrong number of arguments\n", 26);
	else if (index == 2)
		write (2, "Fatal error\n", 12);
	exit(1);
}

void notify_others(int client_fd, char *buffer_write)
{
	for (int fd = 0; fd <= max_fd; fd++)
		if(FD_ISSET(fd, &write_set) && fd != client_fd)
			send (fd, buffer_write, strlen(buffer_write), 0);
}

void send_msg(int client_fd)
{
	char *msg;
	while (extract_message(&(msgs[client_fd]), &msg))
	{
		sprintf(buffer_write, "client %d: ", ids[client_fd]);
		notify_others(client_fd, buffer_write);	

		notify_others(client_fd, msg);
		free(msg);	
	}
}

void remove_client (int client_fd)
{
	sprintf(buffer_write, "server: client %d just left\n", ids[client_fd]);
	notify_others(client_fd, buffer_write);

	free(msgs[client_fd]);
	msgs[client_fd] = NULL;
	FD_CLR(client_fd, &current_set);
	close (client_fd);
}

void register_client(int client_fd)
{
	if (client_fd > max_fd)
		max_fd = client_fd;
	ids[client_fd] = count++;
	msgs[client_fd] = NULL;
	FD_SET(client_fd, &current_set);

	sprintf(buffer_write, "server: client %d just arrived\n", ids[client_fd]);
	notify_others(client_fd, buffer_write);
}

int create_socket(void)
{
	max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (max_fd < 0)
		error_msg(2);

    if (fcntl(max_fd, F_SETFL, O_NONBLOCK) < 0)
        error_msg(2);
	
	FD_SET(max_fd, &current_set);
	return (max_fd);
}

void cleanup(void)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (msgs[fd] != NULL)
		{
			free(msgs[fd]);
			msgs[fd] = NULL;
		}
	}
}

void handle_signal(int signal)
{
	(void)signal;
	cleanup();
	exit(0);
}

int main (int argc, char **argv)
{
	if (argc != 2)
		error_msg(1);

	signal(SIGINT, handle_signal);

	int sockfd = create_socket();

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(argv[1]));

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		error_msg(2);
	if (listen(sockfd, 10))
		error_msg(2);

	while (42)
	{
		read_set = write_set = current_set;
		if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0)
			error_msg(2);

		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &read_set))
				continue ;

			if (fd == sockfd)
			{
				socklen_t len = sizeof(servaddr);
				int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
				if (client_fd >= 0)
				{
					if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
            			error_msg(2);
					
					register_client(client_fd);
					break ;
				}
			}

			else
			{
				int read_bytes = recv(fd, buffer_read, 1023, 0);
				if (read_bytes <= 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						// No data available, continue to the next iteration
						continue;
					}
					else
					{
						// An actual error occurred, remove the client
						remove_client(fd);
						break;
					}
				}
				buffer_read[read_bytes] = '\0';
				msgs[fd] = str_join(msgs[fd], buffer_read);
				send_msg(fd);
			}


		}
	}
	cleanup();
	return (0);
}






