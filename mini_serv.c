
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>

#define BUF_SIZE 4096
#define MAX_CLIENTS 4096

int program = 1;

void sigint_handler(int sig)
{
    (void)sig;
    program = 0;
}

char *strjoin(const char *frst, const char *scnd) {
    char *res;

    res = malloc((strlen(frst) + strlen(scnd)) + 1);
    if (!res) return NULL;

    size_t i = 0;
    while (*frst)
        res[i++] = *frst++;
    while (*scnd)
        res[i++] = *scnd++;
    res[i] = '\0';
    return res;
}

size_t num_len(int num, int len)
{
    if (num == 0)   
        return len;
    return (num_len(num / 10, len + 1));
}

char *my_itoa(int num)
{
    char *res;
    size_t size = num_len(num, 0);
    res = malloc(size * sizeof(char) + 1);
    for (size_t i = 0; i < size; i++) {
        res[size - i - 1] = num % 10 + '0';
        num /= 10;
    }
    res[size] = '\0';
    return res;
}

void send_to_clients(int clients[MAX_CLIENTS], int sender, char *to_send) {
    char *sender_char = my_itoa(sender);
    char *temp = strjoin(sender_char, " : ");
    char *final_msg = strjoin(temp, to_send);
    free(temp);
    temp = final_msg;
    final_msg = strjoin(final_msg, "\n");
    for (size_t i = 1; i < MAX_CLIENTS; i++)
        if (clients[i] != -1 && clients[i] != sender)
            send(clients[i], final_msg, strlen(final_msg), 0);
    free(sender_char);
    free(temp);
    free(final_msg);
}

int main(int argc, char **argv)
{
    char *errors[2] = {"port", "ip address"};
    if (argc < 3) {
        printf("[ERROR] : no %s given\n", errors[argc - 1]);
        return 1;
    }
    if (atoi(argv[1]) <= 0 || atoi(argv[1]) > 65535) {
        printf("[ERROR] : port %s is too large\n", argv[1]);
        return 1;
    }
    signal(SIGINT, sigint_handler);

    struct sockaddr_in serv_addr, cli_addr;
    int opt_val = 1;
    socklen_t cli_len = sizeof(cli_addr);
    int serv_sock;
    if ((serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) { printf("[ERROR] : socket failed\n"); return 1; }

    fd_set mainfds, tempfds;
    int clients[MAX_CLIENTS];
    int nfds = serv_sock;
    
    FD_ZERO(&mainfds);
    FD_SET(serv_sock, &mainfds);
    for (size_t i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;
    clients[0] = serv_sock;

    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) { printf("[ERROR] : setsockopt failed\n"); return 1; }

    bzero(&serv_addr, sizeof(serv_addr));
    bzero(&cli_addr, sizeof(cli_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0) { printf("[ERROR] : wrong ip address\n"); return 1; }

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { printf("[ERROR] : bind failed\n"); return 1; }

    if (listen(serv_sock, 10) < 0) { printf("[ERROR] : listen failed\n"); return 1; }

    while (program)
    {
        tempfds = mainfds;
        int ret = select(nfds + 1, &tempfds, NULL, NULL, NULL);
        if (ret < 0) {
            if (program) return (printf("[ERROR] : select failed\n"), 1);
            else { printf("\r[LOG] : server shutdown...\n"); break; }
        }
        if (FD_ISSET(serv_sock, &tempfds))
        {
            int new_cli_sock;
            if ((new_cli_sock = accept(serv_sock, (struct sockaddr *)&cli_addr, &cli_len)) < 0) { printf("[ERROR] : problem occured with accept when connecting to server\n"); continue; }
            FD_SET(new_cli_sock, &mainfds);
            for (size_t i = 0; i < MAX_CLIENTS; i++)
                if (clients[i] == -1) { clients[i] = new_cli_sock; break; }
            if (new_cli_sock > nfds) nfds = new_cli_sock;
            printf("[LOG] : new client connected with socket %d\n", new_cli_sock);
        }
        for (size_t i = 1; i < MAX_CLIENTS; i++) 
        {
            if (clients[i] != -1 && FD_ISSET(clients[i], &tempfds))
            {
                char buf[BUF_SIZE];
                int r;
                if ((r = recv(clients[i], buf, sizeof(buf), 0)) < 0) { printf("[ERROR] : recv failed for socket %d\n", clients[i]); continue; }
                if (r == 0) {
                    FD_CLR(clients[i], &mainfds);
                    printf("[LOG] : client %d disconnected\n", clients[i]); 
                    close(clients[i]);
                    clients[i] = -1;
                    continue;
                }
                buf[r - 1] = '\0'; // r - 1 to replace the \n
                send_to_clients(clients, clients[i], buf);
                printf("[LOG] : %d sended \"%s\"\n", clients[i], buf);
            }
        }
    }
    for (size_t i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] != -1) { close(clients[i]); clients[i] = -1; }
}