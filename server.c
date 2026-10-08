#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080

int main(void){
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

    printf("Server listening on port %d\n", PORT);

    struct sockaddr_in cli;
    socklen_t cli_len = sizeof(cli);

    int fd = accept(srv, (struct sockaddr*)&cli, &cli_len);

    if(fd < 0){
        perror("accept");

        return 1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));

    printf("Client connected: %s:%d\n", ip, ntohs(cli.sin_port));

    char buffer[1024];
    ssize_t n;

    while((n = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0){
        send(fd, buffer, n, 0);
    }

    if(n < 0){
        perror("recv");
    }else{
        printf("Client disconnected\n");
    }

    close(fd);
    close(srv);
    return 0;
}