#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define DEFAULT_MONITOR_PORT "9090"
#define DEFAULT_MONITOR_HOST "localhost"

int connect_to_monitor(const char *host, const char *port);
int perform_handshake(int sock);
void send_command(int sock, const char *command);
void read_response(int sock);

int main(int argc, char *argv[]) {
    char *host = DEFAULT_MONITOR_HOST;
    char *port = DEFAULT_MONITOR_PORT;
    
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = argv[2];
    }
    
    printf("Connecting to monitor server at %s:%s\n", host, port);
    
    int sock = connect_to_monitor(host, port);
    if (sock < 0) {
        fprintf(stderr, "Failed to connect to monitor server\n");
        return 1;
    }
    
    printf("Connected! Performing handshake...\n");
    
    // Perform handshake
    if (perform_handshake(sock) != 0) {
        fprintf(stderr, "Handshake failed\n");
        close(sock);
        return 1;
    }   
    
    printf("Handshake successful! Type commands (STATS, CONNECTIONS, USERS, CONFIG <param> <value>)\n");
    printf("Type 'quit' to exit\n\n");
    
    char command[256];
    while (1) {
        printf("monitor> ");
        fflush(stdout); // Ensure prompt is displayed
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        if (strcmp(command, "quit") == 0) {
            break;
        }
        
        if (strlen(command) > 0) {
            send_command(sock, command);
            read_response(sock);
        }
    }
    
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
            break; // Success
        }
        
        close(sock);
    }
    
    freeaddrinfo(result);
    
    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        return -1;
    }
    
    return sock;
}

int perform_handshake(int sock) {
    char buffer[BUFFER_SIZE];
    
    // Read server banner
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        fprintf(stderr, "Failed to receive server banner\n");
        return -1;
    }
    buffer[received] = '\0';
    
    printf("Server banner: %s", buffer);
    
    // Check if banner is correct
    if (strncmp(buffer, "SOCKS-MONITOR 1.0", 17) != 0) {
        fprintf(stderr, "Invalid server banner: %s", buffer);
        return -1;
    }
    
    // Ask user to type the handshake response manually
    printf("Please type the handshake response: ");
    fflush(stdout);
    
    char user_input[256];
    while (1) {
        if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
            fprintf(stderr, "Failed to read user input\n");
            return -1;
        }
        
        // Remove newline
        user_input[strcspn(user_input, "\n")] = 0;
        
        // Check if user typed the correct response
        if (strcmp(user_input, "MONITOR-CLIENT 1.0") == 0) {
            // Send the correct response
            const char *response = "MONITOR-CLIENT 1.0\n";
            ssize_t sent = send(sock, response, strlen(response), 0);
            if (sent < 0) {
                fprintf(stderr, "Failed to send handshake response\n");
                return -1;
            }
            printf("Handshake response sent successfully!\n");
            return 0;
        } else {
            printf("Incorrect response. Please try again: ");
            fflush(stdout);
        }
    }
}

void send_command(int sock, const char *command) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s\n", command);
    
    ssize_t sent = send(sock, buffer, strlen(buffer), 0);
    if (sent < 0) {
        perror("send");
    }
}

void read_response(int sock) {
    char buffer[BUFFER_SIZE];
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    if (received > 0) {
        buffer[received] = '\0';
        printf("%s", buffer);
    } else if (received == 0) {
        printf("Server closed connection\n");
    } else {
        perror("recv");
    }
} 