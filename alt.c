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
void handle_command(char *command,NodeData *myNode);
NodeData djoin(NodeData *myNode, char *connectIP, int connectTCP);
int connect_to_node(char *ip, int port);

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

    init_node(&my_id, cache_size, reg_server_ip, reg_server_port); //Inicializa o nó

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
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);  // Clear the buffer before reading new data 
        struct sockaddr addr;
        socklen_t addrlen;
        int new_fd;
        char *ptr;
        char hello[25];
        ptr=strcpy(hello,"Connectado com sucesso!\n");
        for (int i = 0; i <= max_fd; i++) {
            // check if "i" is set as a file descriptor FD_ISSET
            if (FD_ISSET(i, &read_fds)) { 
                // CASE 1: New connection request on the listener socket
                if (i == my_id.socket_listening) {              
                    addrlen = sizeof addr;
                    new_fd = accept(my_id.socket_listening, &addr, &addrlen);
                   
                    write(new_fd,ptr,sizeof(hello));
                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(new_fd, &master_fds); // Add new FD to master set (notice we are not adding it to the read_fds set, 
                                                     // which is the set being altered by the select() function).
                        if (new_fd > max_fd) max_fd = new_fd;
                        printf("New connection established: FD %d\n", new_fd);
                    }
                }
                // CASE 2: Data received from an existing client
                else {
                    int n = read(i, buffer, BUFFER_SIZE);
                    if (n <= 0) {                   // Connection closed or error
                        if (n == 0) {
                            printf("Client on FD %d disconnected.\n", i);
                        } else {
                            perror("read");
                        }
                        close(i);
                        FD_CLR(i, &master_fds);     // Remove closed connection from master set
                    } else {

                        // Echo the message back to the client
                        printf("Received from FD %d: %s\n", i, buffer);
                         // Remover o caractere de nova linha no final do comando
                        //  buffer[strcspn(command, "\n")] = '\0';
                         handle_command(buffer, &my_id);
                        if (write(i, buffer, n) == -1) {
                            perror("write");
                        }
                    }
                }
            }
        }
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

    if ((errcode = getaddrinfo(ip, port_str, &hints, &res)) != 0) {
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

void handle_command(char *command, NodeData *myNode) {
    printf("Comando recebido: %s\n", command);

    if (strcmp(command, "help") == 0) {
        printf("Comandos disponíveis:\n");
        printf("  help - Mostra esta mensagem de ajuda\n");
        printf("  exit - Sai do programa\n");
    } else if (strcmp(command, "exit") == 0) {
        printf("Saindo...\n");
        exit(0);
    } else if (strncmp(command, "djoin", 5) == 0) {
        char connectIP[16];
        int connectTCP;
        if (sscanf(command + 6, "%15s %d", connectIP, &connectTCP) == 2) {
            NodeData newNode;
            newNode = djoin(myNode, connectIP, connectTCP);
        } else {
            printf("Uso correto: djoin connectIP connectTCP\n");
        }
    } else {
        printf("Comando desconhecido: %s\n", command);
    }
}

NodeData djoin(NodeData *myNode, char *connectIP, int connectTCP) {
    NodeData updatedNode = *myNode;
    
    if (strcmp(connectIP, "0.0.0.0") == 0) {
        printf("Criando rede com nó raiz (%s:%d)\n", myNode->ip, myNode->tcp_port);
        memset(&updatedNode.external, 0, sizeof(NodeID));
        memset(&updatedNode.safeguard, 0, sizeof(NodeID));
        updatedNode.numInternals = 0;
    } else {
        int sockfd = connect_to_node(connectIP, connectTCP);
        if (sockfd < 0) {
            printf("Falha ao conectar a %s:%d\n", connectIP, connectTCP);
            return updatedNode;
        }
        printf("Conectado a %s:%d\n", connectIP, connectTCP);
        
        char entry_msg[BUFFER_SIZE];
        snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", myNode->ip, myNode->tcp_port);
        send(sockfd, entry_msg, strlen(entry_msg), 0);
    }
    return updatedNode;
}

int connect_to_node(char *ip, int port) {
    struct sockaddr_in server_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    return sockfd;
}


        