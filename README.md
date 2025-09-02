# 42_exam06

```c
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLI 10

#define fatal_error err("Fatal error\n")
#define num_args_error err("Wrong number of arguments\n")

int program = 1;

void sigint_handler(int sig) {
    (void)sig;
    program = 0;
}

typedef struct client {
	int id;
	int fd;
	char *buf;
	char *msg;
	char to_send[100];
} client;

client clients[MAX_CLI];
char recv_buf[10000000];
fd_set main_fds, write_fds, read_fds;
int max_fd;

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

void serv_shutdown()
{
	for (size_t i = 0; i < MAX_CLI; i++)
		if (clients[i].fd != -1) { close(clients[i].fd); free(clients[i].buf); }
}

void err(char *msg)
{
	serv_shutdown();
	write(2, msg, strlen(msg));
	exit(1);
}

void send_msg_to_clients(char *msg, int sender)
{
	int msg_len = strlen(msg);
	for (size_t i = 1; i < MAX_CLI; i++)
		if (clients[i].id != -1 && clients[i].id != sender)
		{
			size_t total_sent = 0;
			while (total_sent < msg_len)
			{
				size_t sent = send(clients[i].fd, msg, strlen(msg), 0);
				if (sent < 0) fatal_error;
				total_sent += sent;
			}
		}
	memset(msg, 0, strlen(msg));
}

int main(int argc, char **argv) 
{
	if (argc != 2) num_args_error;
	signal(SIGINT, sigint_handler);

	int serv_fd, cli_fd;
	struct sockaddr_in serv_addr, cli_addr; 
	socklen_t cli_addr_len = sizeof(cli_addr);
	
	serv_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (serv_fd == -1) fatal_error;
	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	serv_addr.sin_port = htons(atoi(argv[1])); 

	FD_ZERO(&main_fds);
	FD_SET(serv_fd, &main_fds);
	max_fd = serv_fd;

	memset(clients, -1, sizeof(clients));
	clients[0].id = -2;
	clients[0].fd = serv_fd;
	clients[0].buf = NULL;
	for (size_t i = 1; i < MAX_CLI; i++)
	{
		clients[i].buf = NULL;
		clients[i].msg = NULL;
		memset(clients[i].to_send, 0, sizeof(clients[i].to_send));
	}

	if ((bind(serv_fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr))) != 0) fatal_error;
	if (listen(serv_fd, 10) != 0) fatal_error;

	while (program)
	{
		read_fds = write_fds = main_fds;
		if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) == -1) fatal_error;
		for (size_t i = 0; i < MAX_CLI; i++)
		{
			if (!FD_ISSET(clients[i].fd, &read_fds)) continue;
			if (clients[i].fd == serv_fd)
			{
				cli_fd = accept(clients[i].fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
				if (cli_fd == -1) fatal_error;
				FD_SET(cli_fd, &main_fds);
				for (size_t i = 0; i < MAX_CLI; i++)
					if (clients[i].id == -1) 
					{ 
						clients[i].id = i - 1; 
						clients[i].fd = cli_fd; 
						sprintf(clients[i].to_send, "server: client %d has just arrived\n", clients[i].id);
						send_msg_to_clients(clients[i].to_send, clients[i].id);
						break; 
					}
				max_fd = cli_fd > max_fd ? cli_fd : max_fd;
			}
			else
			{
				int r = recv(clients[i].fd, recv_buf, 4096, 0);
				if (r <= 0)
				{
					FD_CLR(clients[i].fd, &main_fds);
					close(clients[i].fd); clients[i].fd = -1;
					free(clients[i].buf);
					sprintf(clients[i].to_send, "server: client %d has left\n", clients[i].id);
					send_msg_to_clients(clients[i].to_send, clients[i].id);
				}
				else
				{
					recv_buf[r] = '\0';
					clients[i].buf = str_join(clients[i].buf, recv_buf);
					while (extract_message(&clients[i].buf, &clients[i].msg))
					{
						sprintf(clients[i].to_send, "client %d: ", clients[i].id);
						send_msg_to_clients(clients[i].to_send, clients[i].id);
						send_msg_to_clients(clients[i].msg, clients[i].id);
						free(clients[i].msg);
					}
					memset(recv_buf, 0, sizeof(recv_buf));
				}
			}
		}
	}
	serv_shutdown();
	return (0);
}
```
