#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "tcpServerUtil.h"
#include "logger.h"
#include "util.h"

#define MAXPENDING 5 // Maximum outstanding connection requests
#define BUFSIZE 256
#define MAX_ADDR_BUFFER 128

static char addrBuffer[MAX_ADDR_BUFFER];
/*
 ** Se encarga de resolver el número de puerto para service (puede ser un string con el numero o el nombre del servicio)
 ** y crear el socket pasivo, para que escuche en cualquier IP, ya sea v4 o v6
 */
int setupTCPServerSocket(const char *service) {
	// Construct the server address structure
	struct addrinfo addrCriteria;                   // Criteria for address match
	memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
	addrCriteria.ai_family = AF_UNSPEC;             // Any address family
	addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
	addrCriteria.ai_socktype = SOCK_STREAM;         // Only stream sockets
	addrCriteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol

	struct addrinfo *servAddr; 			// List of server addresses
	int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
	if (rtnVal != 0) {
		log(FATAL, "getaddrinfo() failed %s", gai_strerror(rtnVal));
		return -1;
	}

	int servSock = -1;
	// Intentamos ponernos a escuchar en alguno de los puertos asociados al servicio, sin especificar una IP en particular
	// Iteramos y hacemos el bind por alguna de ellas, la primera que funcione, ya sea la general para IPv4 (0.0.0.0) o IPv6 (::/0) .
	// Con esta implementación estaremos escuchando o bien en IPv4 o en IPv6, pero no en ambas
	for (struct addrinfo *addr = servAddr; addr != NULL && servSock == -1; addr = addr->ai_next) {
		errno = 0;
		// Create a TCP socket
		servSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (servSock < 0) {
			log(DEBUG, "Cant't create socket on %s : %s ", printAddressPort(addr, addrBuffer), strerror(errno));  
			continue;       // Socket creation failed; try next address
		}

		// Bind to ALL the address and set socket to listen
		if ((bind(servSock, addr->ai_addr, addr->ai_addrlen) == 0) && (listen(servSock, MAXPENDING) == 0)) {
			// Print local address of socket
			struct sockaddr_storage localAddr;
			socklen_t addrSize = sizeof(localAddr);
			if (getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize) >= 0) {
				printSocketAddress((struct sockaddr *) &localAddr, addrBuffer);
				log(INFO, "Binding to %s", addrBuffer);
			}
		} else {
			log(DEBUG, "Cant't bind %s", strerror(errno));  
			close(servSock);  // Close and try with the next one
			servSock = -1;
		}
	}

	freeaddrinfo(servAddr);

	return servSock;
}

int acceptTCPConnection(int servSock) {
	struct sockaddr_storage clntAddr; // Client address
	// Set length of client address structure (in-out parameter)
	socklen_t clntAddrLen = sizeof(clntAddr);

	// Wait for a client to connect
	int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
	if (clntSock < 0) {
		log(ERROR, "accept() failed");
		return -1;
	}

	// clntSock is connected to a client!
	printSocketAddress((struct sockaddr *) &clntAddr, addrBuffer);
	log(INFO, "Handling client %s", addrBuffer);

	return clntSock;
}

int handleTCPEchoClient(int clntSocket) {
	char buffer[BUFSIZE]; // Buffer for echo string
	// Receive message from client
	ssize_t numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
	if (numBytesRcvd < 0) {
		log(ERROR, "recv() failed");
		return -1;   // TODO definir codigos de error
	}

	// Send received string and receive again until end of stream
	while (numBytesRcvd > 0) { // 0 indicates end of stream
		// Echo message back to client
		ssize_t numBytesSent = send(clntSocket, buffer, numBytesRcvd, 0);
		if (numBytesSent < 0) {
			log(ERROR, "send() failed");
			return -1;   // TODO definir codigos de error
		}
		else if (numBytesSent != numBytesRcvd) {
			log(ERROR, "send() sent unexpected number of bytes ");
			return -1;   // TODO definir codigos de error
		}

		// See if there is more data to receive
		numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
		if (numBytesRcvd < 0) {
			log(ERROR, "recv() failed");
			return -1;   // TODO definir codigos de error
		}
	}

	close(clntSocket);
	return 0;
}

typedef struct {
	char buffer[BUFSIZE];
	size_t read_bytes;
} echo_data;

static const struct fd_handler echo_handler = {
	.handle_read = echo_read,
	.handle_close = echo_close,
};

// Non blocking accept
void accept_connection(struct selector_key *key) {
	int server_fd = key->fd;
	fd_selector selector = key->data;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	// Accept a new connection
	int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

	// Check if accept was successful
	if(client_fd < 0) {
		log(ERROR, "accept() failed: %s", strerror(errno));
		return;
	}

	// Set the client socket to non-blocking
	if(selector_fd_set_nio(client_fd) == -1) {
		log(ERROR, "Failed to set client socket to non-blocking: %s", strerror(errno));
		close(client_fd);
		return;
	}

	echo_data *data = malloc(sizeof(*data));
	if(data == NULL) {
		log(ERROR, "malloc() failed");
		close(client_fd);
		return;
	}
	data->read_bytes = 0;

	// Register the client socket with the selector
	if(selector_register(selector, client_fd, &echo_handler, OP_READ, data) != SELECTOR_SUCCESS) {
		log(ERROR, "selector_register() failed for client");
		close(client_fd);
		free(data);
		return;
	}

	log(INFO, "New client connected: fd=%d", client_fd);
}

void echo_read(struct selector_key *key) {
	echo_data *data = key->data;
	ssize_t n = recv(key->fd, data->buffer, BUFSIZE, 0);

	if(n > 0) {
		send(key->fd, data->buffer, n, 0);  // echo
	} else if (n == 0 || (n < 0 && errno != EAGAIN)) {
		selector_unregister_fd(key->s, key->fd);
		close(key->fd);
		//free(data);
		log(INFO, "Client closed connection: fd=%d", key->fd);
	}
}

void echo_close(struct selector_key *key) {
	if(key->data != NULL) {
		free(key->data);
	}
	close(key->fd);
}
