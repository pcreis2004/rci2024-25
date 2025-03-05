#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>  

#define BUFFER_SIZE 128

typedef struct NodeID {
    char ip[16];
    int tcp_port;
    int socket_listening;
    struct NodeID *next;
} NodeID;

typedef struct {
    char ip[16];
    int tcp_port;
    int socket_listening;
    NodeID external;
    NodeID safeguard;
    NodeID *internal;
    int numInternals;
    int capacityInternals;
} NodeData;

void init_node(NodeData *myNode, int cache_size, char *reg_ip, int reg_udp);
int init_socket_listening(int port, char *ip);
int handle_connection(NodeData *myNode, fd_set *master_fds, int socket_fd);

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s cache IP TCP [regIP] [regUDP]\n", argv[0]);
        return 1;
    }

    NodeData my_id;
    // Initialize node details
    strncpy(my_id.ip, argv[2], sizeof(my_id.ip) - 1);
    my_id.ip[sizeof(my_id.ip) - 1] = '\0';
    my_id.tcp_port = atoi(argv[3]);

    char reg_server_ip[16] = "193.136.138.142";  // Default registration server IP
    int reg_server_port = 59000;  // Default registration server port
    int cache_size = atoi(argv[1]);  // Cache size

    // Handle optional registration server IP and port
    if (argc >= 5) {
        strncpy(reg_server_ip, argv[4], sizeof(reg_server_ip) - 1);
        reg_server_ip[sizeof(reg_server_ip) - 1] = '\0';
    }
    if (argc >= 6) {
        reg_server_port = atoi(argv[5]);
    }

    // Print configuration
    printf("Configuration:\n Cache: %d\n IP: %s\n TCP: %d\n RegIP: %s\n RegUDP: %d\n",
           cache_size, my_id.ip, my_id.tcp_port, reg_server_ip, reg_server_port);

    printf("Initializing node...\n");
    init_node(&my_id, cache_size, reg_server_ip, reg_server_port);

    printf("NDN Node started. Enter commands:\n");

    // File descriptor management
    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(my_id.socket_listening, &master_fds);
    int max_fd = my_id.socket_listening;

    while (1) {
        // Copy the master set to the read set
        read_fds = master_fds;
        printf("> ");
        fflush(stdout);
        
        // Wait for activity on any socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity == -1) {
            perror("select");
            exit(1);
        }

        // Iterate through file descriptors
        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                // If it's the listening socket, handle new connection
                if (i == my_id.socket_listening) {
                    struct sockaddr_storage remoteaddr;
                    socklen_t addrlen = sizeof(remoteaddr);
                    int new_fd = accept(my_id.socket_listening, 
                                        (struct sockaddr *)&remoteaddr, 
                                        &addrlen);
                    
                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        // Add new connection to master set
                        FD_SET(new_fd, &master_fds);
                        if (new_fd > max_fd) {
                            max_fd = new_fd;
                        }
                        printf("New connection established: FD %d\n", new_fd);
                    }
                } 
                // If it's an existing connection, handle data
                else {
                    int result = handle_connection(&my_id, &master_fds, i);
                    if (result == 0) {
                        // Connection closed, update max_fd if needed
                        if (i == max_fd) {
                            while (max_fd >= 0 && !FD_ISSET(max_fd, &master_fds)) 
                                max_fd--;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

void init_node(NodeData *myNode, int cache_size, char *reg_ip, int reg_udp) {
    // Initialize node data structures
    memset(&myNode->external, 0, sizeof(NodeID));
    memset(&myNode->safeguard, 0, sizeof(NodeID));
    myNode->internal = NULL;
    myNode->numInternals = 0;
    myNode->capacityInternals = 0;

    // Create listening socket
    myNode->socket_listening = init_socket_listening(myNode->tcp_port, myNode->ip);
}

int init_socket_listening(int port, char *ip) {
    struct addrinfo hints, *res;
    int fd, errcode;
    char port_str[6];

    sprintf(port_str, "%d", port);

    // Create socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Socket options to reuse address
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // Prepare address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Get address info
    if ((errcode = getaddrinfo(ip, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
        exit(1);
    }

    // Bind socket
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(res);
        exit(1);
    }

    freeaddrinfo(res);

    // Start listening
    if (listen(fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Node listening on port %s...\n", port_str);
    return fd;
}

int handle_connection(NodeData *myNode, fd_set *master_fds, int socket_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Read data from socket
    int bytes_read = read(socket_fd, buffer, BUFFER_SIZE - 1);
    
    // Handle read errors or disconnection
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("Client on FD %d disconnected\n", socket_fd);
        } else {
            perror("read");
        }
        
        // Close socket and remove from master set
        close(socket_fd);
        FD_CLR(socket_fd, master_fds);
        return 0;
    }

    // Null-terminate the buffer to ensure safe string handling
    buffer[bytes_read] = '\0';

    // Process received data
    printf("Received from FD %d: %s\n", socket_fd, buffer);
    
    // Echo back the message
    if (write(socket_fd, buffer, bytes_read) == -1) {
        perror("write");
    }

    return 1;
}