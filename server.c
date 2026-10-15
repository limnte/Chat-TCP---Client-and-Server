#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_CLIENTS 64

static struct pollfd fds[MAX_CLIENTS + 1]; // +1 for the listening socket
static char info[MAX_CLIENTS + 1][INET_ADDRSTRLEN + 8]; // "ip:port" of each client
static int nfds = 0; // number of active file descriptors

static int send_all(int fd, const char *buf, size_t len){
    size_t sent = 0;

    while(sent < len){
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);

        if(n < 0){
            if(errno == EINTR) continue; // interrupted by signal, retry
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static void add_client(int fd, const char *who){
    fds[nfds].fd = fd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;

    snprintf(info[nfds], sizeof(info[nfds]), "%s", who);
    nfds++;
}

// Swap-remove: move the last entry into slot i, so there are no holes
static void remove_client(int i){
    close(fds[i].fd);                                   
    fds[i] = fds[nfds - 1];
    memcpy(info[i], info[nfds - 1], sizeof(info[i]));
    nfds--;
}

static void broadcast(int from, const char *data, size_t len){
    char prefix[64];

    int plen = snprintf(prefix, sizeof(prefix), "[%s] ", info[from]);

    for(int j = 1; j < nfds; j++){
        if(j == from) continue;

        send_all(fds[j].fd, prefix, (size_t)plen);
        send_all(fds[j].fd, data, len);
    }
}


int main(void){
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE to prevent crashes on send to closed sockets

    int srv = socket(AF_INET, SOCK_STREAM, 0); //IPV4, TCP

    if(srv < 0){
        perror("socket");

        return 1;
    }

    // Allow socket to be reused immediately after program exits
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //on all available interfaces
    addr.sin_port = htons(PORT); // host -> network byte order

    if(bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");

        return 1;
    }

    if(listen(srv, 10) < 0){
        perror("listen");

        return 1;
    }

    fds[0].fd = srv;
    fds[0].events = POLLIN;
    snprintf(info[0], sizeof(info[0]), "Listener");
    nfds = 1;

    printf("Chat Server listening on port %d (poll, single process)\n", PORT);

    char buf[1024];

    for(;;){
        if(poll(fds, (nfds_t)nfds, -1) < 0){
            if(errno == EINTR) continue; // interrupted by signal, retry
            perror("poll");
            break;
        }

        // New connection
        if(fds[0].revents & POLLIN){
            struct sockaddr_in cli;
            socklen_t cli_len = sizeof(cli);

            int fd = accept(srv, (struct sockaddr*)&cli, &cli_len);

            if(fd < 0){
                if(errno != EINTR && errno != ECONNABORTED && errno != EAGAIN){
                    perror("accept");
                }
            }else if(nfds >= MAX_CLIENTS + 1){
                const char *msg = "Server full, try again later.\n";
                send_all(fd, msg, strlen(msg));
                close(fd);
            }else{
                char ip[INET_ADDRSTRLEN], who[INET_ADDRSTRLEN + 8];
                inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
                snprintf(who, sizeof(who), "%s:%d", ip, ntohs(cli.sin_port));
                add_client(fd, who);
                printf("New connection from %s (total: %d)\n", who, nfds - 1);
            }
        }

       
        for(int i = nfds - 1; i >= 1; i--){                // FIX: nfds - 1
            if(!(fds[i].revents & (POLLIN | POLLERR | POLLHUP))) continue;

            ssize_t n = recv(fds[i].fd, buf, sizeof(buf), 0);

            if(n <= 0){
                if(n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                printf("Client disconnected: %s\n", info[i]);
                remove_client(i);
                continue;
            }
            broadcast(i, buf, (size_t)n);
        }
    }

    close(srv);
    return 0;
}