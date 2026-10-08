#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080

int main(void){
    int fd = socket(AF_INET, SOCK_STREAM, 0); //IPV4, TCP
    
    if(fd < 0){
        perror("socket");
        
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.01", &addr.sin_addr);

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("connect");

        return 1;
    }

    printf("Connected to server on port %d\n\n", PORT);
    printf("Enter a message to send to the server(CTRL+D to quit): ");

    char line [1024], buffer[1024];

    while(fgets(line, sizeof(line), stdin)){
        send(fd, line, strlen(line), 0);
        ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);

        if(n < 0){
            perror("recv");
            break;
        }

        buffer[n] = '\0';
        printf("echo: %s\n", buffer);
    }

    close(fd);
    return 0;
}