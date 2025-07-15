#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../utils/logger.h"

#define BUFFER_SIZE 1024
#define MAX_COMMAND_LENGTH 256
#define DEFAULT_MONITOR_PORT "9090"
#define DEFAULT_MONITOR_HOST "localhost"
#define PROMPT "monitor>"
#define OK_RESPONSE_PREFIX "OK"
#define OK_RESPONSE_PREFIX_LEN 2
#define QUIT_COMMAND "quit"
#define INITIAL_MESSAGE "Type commands and press Enter. Type 'quit' to exit.\n\n"


int connect_to_monitor(const char *host, const char *port);
int authenticate_user(int sock);
void transmit_mode(int sock);

int main(int argc, char *argv[]) {
    char *host = DEFAULT_MONITOR_HOST;
    char *port = DEFAULT_MONITOR_PORT;
    
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = argv[2];
    }
    
    int sock = connect_to_monitor(host, port);
    if (sock < 0) {
        perror("Failed to connect to monitor server\n");
        return 1;
    }
    
    if (authenticate_user(sock) != 0) {
        close(sock);
        return 1;
    }   
    
    printf(INITIAL_MESSAGE);
    
    transmit_mode(sock);
    
    close(sock);
    printf("Disconnected from monitor server\n");
    return 0;
}

int connect_to_monitor(const char *host, const char *port) {
    struct addrinfo hints, *result, *rp;
    int sock;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    
    int s = getaddrinfo(host, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) {
            continue;
        }
        
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }
        
        close(sock);
    }
    
    freeaddrinfo(result);
    
    if (rp == NULL) {
        perror("Could not connect\n");
        return -1;
    }
    
    return sock;
}

int authenticate_user(int sock) {
    char buffer[BUFFER_SIZE];
    
    ssize_t received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (received <= 0) {
        perror("Failed to receive server banner\n");
        return -1;
    }
    buffer[received] = '\0';
    
    printf("%s", buffer);
    fflush(stdout);
    
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
        perror("Failed to read user input\n");
        return -1;
    }
    size_t credentials_len = strcspn(buffer, "\n");
    buffer[credentials_len] = '\n';
    credentials_len++;
    buffer[credentials_len + 1] = '\0';
    
    ssize_t sent = send(sock, buffer, credentials_len, 0);
    if (sent < 0) {
        perror("send");
        return -1;
    }
    
    received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (received <= 0) {
        perror("Failed to receive server response\n");
        return -1;
    }
    buffer[received] = '\0'; 
    printf("%s", buffer);
    if (strncmp(buffer, OK_RESPONSE_PREFIX, OK_RESPONSE_PREFIX_LEN) != 0) {
       //if (recv(sock, buffer, BUFFER_SIZE - 1, 0) <= 0) {
            printf("Server closed connection\n");
        //}
        return -1;
    }
    return 0;
}

void transmit_mode(int sock) {
    char buffer[BUFFER_SIZE];
    char command[MAX_COMMAND_LENGTH];
    
    while (1) {
        printf(PROMPT);
        fflush(stdout);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        size_t command_len = strcspn(command, "\n");
        command[command_len] = 0;
        
        if (command_len > 0) {
            if (strcmp(command, QUIT_COMMAND) == 0) {
                break;
            }
            
            if (strlen(command) > 0) {
                char message[512];
                snprintf(message, sizeof(message), "%s\n", command);
                ssize_t sent = send(sock, message, strlen(message), 0);
                if (sent < 0) {
                    perror("send");
                    break;
                }
                
                ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (received > 0) {
                    buffer[received] = '\0';
                    printf("%s", buffer);
                } else if (received == 0) {
                    printf("Server closed connection\n");
                    break;
                } else {
                    perror("recv");
                    break;
                }
            }
        }
    }
} 