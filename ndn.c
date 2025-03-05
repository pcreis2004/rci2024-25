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
void checkFD(NodeData *myNode, fd_set *master_fds, int *max_fd);

int main(int argc, char *argv[]) {
    char command[256];

    if (argc < 4) {
        printf("Usage: %s cache IP TCP [regIP] [regUDP]\n", argv[0]);
        return 1;
    }

    NodeData my_id;
    strncpy(my_id.ip, argv[2], sizeof(my_id.ip) - 1);
    my_id.ip[sizeof(my_id.ip) - 1] = '\0';  // Garantir terminação correta
    my_id.tcp_port = atoi(argv[3]);

    char reg_server_ip[16];      // Registration server IP
    int reg_server_port;         // Registration server UDP port
    int cache_size = atoi(argv[1]);  // Cache size

    if (argc >= 5) {
        strncpy(reg_server_ip, argv[4], sizeof(reg_server_ip) - 1);
        reg_server_ip[sizeof(reg_server_ip) - 1] = '\0';
    } else {
        strncpy(reg_server_ip, "193.136.138.142", sizeof(reg_server_ip) - 1);
    }

    if (argc >= 6) {
        reg_server_port = atoi(argv[5]);
    } else {
        reg_server_port = 59000;
    }

    printf("Configuração:\n Cache: %d\n IP: %s\n TCP: %d\n RegIP: %s\n RegUDP: %d\n",
           cache_size, my_id.ip, my_id.tcp_port, reg_server_ip, reg_server_port);

    printf("Inicializando nó...\n");
    init_node(&my_id, cache_size, reg_server_ip, reg_server_port);

    printf("NDN Node started. Enter commands:\n");

    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(my_id.socket_listening, &master_fds);
    int max_fd = my_id.socket_listening;

    while (1) {
        read_fds = master_fds;
        printf("> ");
        
        int counter = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (counter == -1) {
            perror("select");
            exit(1);
        }
        
        checkFD(&my_id, &master_fds, &max_fd);
    }

    return 0;
}

void init_node(NodeData *myNode, int cache_size, char *reg_ip, int reg_udp) {
    memset(&myNode->external, 0, sizeof(NodeID));
    memset(&myNode->safeguard, 0, sizeof(NodeID));
    myNode->internal = NULL;
    myNode->numInternals = 0;
    myNode->capacityInternals = 0;

    myNode->socket_listening = init_socket_listening(myNode->tcp_port, myNode->ip);
}

int init_socket_listening(int port, char *ip) {
    struct addrinfo hints, *res;
    int fd, errcode;
    char port_str[6];

    sprintf(port_str, "%d", port);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((errcode = getaddrinfo(NULL, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
        exit(1);
    }

    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(res);
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Node listening on port %s...\n", port_str);
    return fd;
}

void checkFD(NodeData *myNode, fd_set *master_fds, int *max_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    struct sockaddr addr;
    socklen_t addrlen;
    int new_fd;

    for (int i = 0; i <= *max_fd; i++) {
        if (FD_ISSET(i, master_fds)) {
            if (i == myNode->socket_listening) {
                addrlen = sizeof addr;
                new_fd = accept(myNode->socket_listening, &addr, &addrlen);
                if (new_fd == -1) {
                    perror("accept");
                } else {
                    FD_SET(new_fd, master_fds);
                    if (new_fd > *max_fd) *max_fd = new_fd;
                    printf("New connection established: FD %d\n", new_fd);
                }
            } else {
                int n = read(i, buffer, BUFFER_SIZE);
                if (n <= 0) {
                    if (n == 0) {
                        printf("Client on FD %d disconnected.\n", i);
                    } else {
                        perror("read");
                    }
                    close(i);
                    FD_CLR(i, master_fds);
                } else {
                    printf("Received from FD %d: %s\n", i, buffer);
                    if (write(i, buffer, n) == -1) {
                        perror("write");
                    }
                }
            }
        }
    }
}
