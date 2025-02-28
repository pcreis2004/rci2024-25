#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Forward declaration for NodeID struct (for self-referential pointer)
typedef struct NodeID NodeID;

// Define the NodeID struct
struct NodeID {
    char ip[16];
    int tcp_port;
    int socket;
    NodeID *next;
};

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

// Global variables
NodeData myNode;
char reg_server_ip[16]; // Registration server IP
int reg_server_port;    // Registration server UDP port
int cache_size;         // Cache size
int udp_socket;         // UDP socket for registration server

// Function prototypes
void init_node(int cache_size, char *ip, int tcp_port, char *reg_ip, int reg_udp);
int init_socket_listening(int port);
void process_user_command(char *command);
int setup_udp_client();

int main(int argc, char *argv[]) {
    char command[256];
    
    if (argc < 4) {
        printf("Usage: %s cache IP TCP [regIP] [regUDP]\n", argv[0]);
        return 1;
    }
    
    // Parse arguments
    cache_size = atoi(argv[1]);
    
    strncpy(myNode.ip, argv[2], sizeof(myNode.ip));
    myNode.tcp_port = atoi(argv[3]);
    
    if (argc >= 5) {
        strncpy(reg_server_ip, argv[4], sizeof(reg_server_ip));
    } else {
        strncpy(reg_server_ip, "193.136.138.142", sizeof(reg_server_ip));
    }
    
    if (argc >= 6) {
        reg_server_port = atoi(argv[5]);
    } else {
        reg_server_port = 59000;
    }
    
    printf("Configuration:\n Cache: %d\n IP: %s\n TCP: %d\n RegIP: %s\n RegUDP: %d\n",
           cache_size, myNode.ip, myNode.tcp_port, reg_server_ip, reg_server_port);
    
    // Initialize node
    init_node(cache_size, myNode.ip, myNode.tcp_port, reg_server_ip, reg_server_port);
    
    // Main command loop
    printf("NDN Node started. Enter commands:\n");
    while (1) {
        printf("> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        // Process command
        process_user_command(command);
    }
    
    return 0;
}

void init_node(int cache_size, char *ip, int tcp_port, char *reg_ip, int reg_udp) {
    // Initialize topology information
    memset(&myNode.external, 0, sizeof(NodeID));
    memset(&myNode.safeguard, 0, sizeof(NodeID));
    myNode.internal = NULL;
    myNode.numInternals = 0;
    myNode.capacityInternals = 0;
    
    // Initialize listening TCP socket
    myNode.socket_listening = init_socket_listening(tcp_port);
    if (myNode.socket_listening < 0) {
        fprintf(stderr, "Failed to initialize listening socket\n");
        exit(1);
    }
    
    // Initialize UDP socket for communication with registration server
    udp_socket = setup_udp_client();
    if (udp_socket < 0) {
        fprintf(stderr, "Failed to initialize UDP socket\n");
        exit(1);
    }
    
    printf("Node initialized with ID: %s %d\n", ip, tcp_port);
    printf("Listening socket: %d\n", myNode.socket_listening);
}

int init_socket_listening(int port) {
    int sock;
    struct sockaddr_in server_addr;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    // Enable address reuse
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sock);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }
    
    if (listen(sock, 5) < 0) {
        perror("listen");
        close(sock);
        return -1;
    }
    
    printf("Listening on port %d\n", port);
    return sock;
}

int setup_udp_client() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    return sock;
}

void process_user_command(char *command) {
    char cmd[32];
    
    // Parse command
    if (sscanf(command, "%s", cmd) < 1) {
        return;
    }
    
    // Handle commands (just echoing for now)
    printf("Command received: %s\n", command);
    
    // Implement actual command handling here
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "x") == 0) {
        printf("Exiting application...\n");
        // Clean up resources
        close(myNode.socket_listening);
        close(udp_socket);
        exit(0);
    }
}