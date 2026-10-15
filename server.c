#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define PORT 8080

void sigchld_handler(int s){
    (void)s; // suppress unused variable warning
    while(waitpid(-1, NULL, WNOHANG) > 0);
}




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

    signal(SIGCHLD, sigchld_handler); // reap dead child processes

    printf("Server listening on port %d (Multi-client with fork)\n", PORT);

    while(1){
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);

        int fd = accept(srv, (struct sockaddr*)&cli, &cli_len);
        if(fd < 0){
            if(errno == EINTR || errno == ECONNABORTED) continue; // interrupted by signal, retry
            perror("accept");
            return 1;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));

        printf("Client connected: %s:%d\n", ip, ntohs(cli.sin_port));
        
        pid_t pid = fork();
        if(pid < 0){
            perror("fork");
            close(fd);

            continue;
        }

        if(pid == 0){
            //Child process
            close(srv);

            char buffer[1024];
            ssize_t n;

            while((n = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0){
                send(fd, buffer, n, 0);
            }

            printf("Client disconnected: %s:%d\n", ip, ntohs(cli.sin_port));
            close(fd);
            return 0;
        }else{
            //Parent process
            close(fd);
        }
    }

    close(srv);
    return 0;
}